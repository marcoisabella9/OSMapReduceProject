// parallel_sort.cpp
// Simple MapReduce-style parallel sort.

// TO USE: ./parallel_sort --mode thread|proc --workers N --size S

#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
using clk = chrono::high_resolution_clock;

long get_rss_kb(){
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss; // kb on linux
}

void merge_ranges(vector<int> &a, int l, int m, int r, vector<int> &tmp){
    int i=l, j=m, k=l;
    while(i<m && j<r) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while(i<m) tmp[k++] = a[i++];
    while(j<r) tmp[k++] = a[j++];
    for(int t=l;t<r;++t) a[t]=tmp[t];
}

int main(int argc, char** argv){
    string mode="thread";
    int workers=4;
    int N=131072;
    for(int i=1;i<argc;i++){
        string s=argv[i];
        if(s=="--mode") mode=argv[++i];
        else if(s=="--workers") workers=stoi(argv[++i]);
        else if(s=="--size") N=stoi(argv[++i]);
    }
    if(workers < 1) workers = 1;
    cout<<"Mode: "<<mode<<", workers="<<workers<<", size="<<N<<"\n";

    // generate data
    vector<int> local;
    local.reserve(N);
    std::mt19937_64 rng(12345);
    for(int i=0;i<N;++i) local.push_back((int)(rng() & 0x7fffffff));

    // for process mode, use shared memory backing
    int *shared_arr = nullptr;
    size_t bytes = N * sizeof(int);
    int shm_fd = -1;
    if(mode=="proc"){
        // create anonymous shared mapping using mmap (MAP_SHARED | MAP_ANONYMOUS)
        void *p = mmap(nullptr, bytes, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(p==MAP_FAILED){ perror("mmap"); return 1; }
        shared_arr = (int*)p;
        // copy data to shared array
        for(int i=0;i<N;++i) shared_arr[i]=local[i];
    }

    auto map_start = clk::now();
    vector<pair<int,int>> ranges; // [l, r)
    for(int i=0;i<workers;i++){
        int l = (long long)i * N / workers;
        int r = (long long)(i+1) * N / workers;
        ranges.push_back({l,r});
    }

    if(mode=="thread"){
        // threads sort their chunk
        vector<thread> thr;
        for(int i=0;i<workers;i++){
            int l = ranges[i].first, r = ranges[i].second;
            thr.emplace_back([l,r,&local](){
                sort(local.begin()+l, local.begin()+r);
            });
        }
        for(auto &t: thr) t.join();
    } else {
        // processes sort in-place in shared_arr
        for(int i=0;i<workers;i++){
            pid_t pid = fork();
            if(pid==0){
                int l = ranges[i].first, r = ranges[i].second;
                sort(shared_arr + l, shared_arr + r);
                _exit(0);
            }
        }
        // parent waits
        for(int i=0;i<workers;i++) wait(nullptr);
    }
    auto map_end = clk::now();

    // reduce phase: merge sorted chunks into final sorted array in parent
    auto reduce_start = clk::now();
    if(mode=="thread"){
        // inplace pairwise merge using temporary buffer
        vector<int> tmp(N);
        // iterative merging: merge chunk 0..k, k..next etc
        // do simple linear merge of all ranges into tmp and then copy back iteratively
        // perform pairwise merging like tournament merging for simplicity
        // uild vector of (l,r) sorted segments, then repeatedly merge first two
        deque<pair<int,int>> segs;
        for(auto &p: ranges) if(p.first < p.second) segs.push_back(p);
        vector<int> arr = local;
        while(segs.size() > 1){
            auto a = segs.front(); segs.pop_front();
            auto b = segs.front(); segs.pop_front();
            int l = a.first, m = a.second, r = b.second;
            merge_ranges(arr, l, m, r, tmp);
            segs.push_front({l, r});
        }
        // arr is now fully sorted
        local.swap(arr);
    } else {
        // for processes, shared_arr contains sorted segments do merges in parent to produce final vector
        vector<int> tmp(N);
        deque<pair<int,int>> segs;
        for(auto &p: ranges) if(p.first < p.second) segs.push_back(p);
        // copy shared_arr into vector<int> arr
        vector<int> arr(N);
        for(int i=0;i<N;++i) arr[i]=shared_arr[i];
        while(segs.size() > 1){
            auto a = segs.front(); segs.pop_front();
            auto b = segs.front(); segs.pop_front();
            int l = a.first, m = a.second, r = b.second;
            merge_ranges(arr, l, m, r, tmp);
            segs.push_front({l, r});
        }
        // arr contains sorted result
        // optionally write back to shared_arr
        for(int i=0;i<N;++i) shared_arr[i]=arr[i];
    }
    auto reduce_end = clk::now();

    // verify sorted
    bool ok=true;
    if(mode=="thread"){
        for(int i=1;i<N;i++) if(local[i-1] > local[i]) { ok=false; break; }
    } else {
        for(int i=1;i<N;i++) if(shared_arr[i-1] > shared_arr[i]) { ok=false; break; }
    }

    auto map_ms = chrono::duration_cast<chrono::milliseconds>(map_end - map_start).count();
    auto reduce_ms = chrono::duration_cast<chrono::milliseconds>(reduce_end - reduce_start).count();
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(reduce_end - map_start).count();

    cout<<"Map time (ms): "<<map_ms<<"\n";
    cout<<"Reduce time (ms): "<<reduce_ms<<"\n";
    cout<<"Total time (ms): "<<total_ms<<"\n";
    cout<<"Sorted OK: "<<(ok ? "yes":"NO")<<"\n";
    cout<<"Peak RSS (KB): "<<get_rss_kb()<<"\n";

    if(mode=="proc"){
        munmap(shared_arr, bytes);
    }
    return 0;
}
