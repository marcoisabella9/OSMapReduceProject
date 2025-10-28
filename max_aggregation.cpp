// max_aggregation.cpp
// Compute global maximum with worker map phase and single reducer
// Shared memory limited to a single integer holding current global max

// TO USE: ./max_aggregation --mode thread|proc --workers N --size NUM_ELEMENTS

#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
using clk = chrono::high_resolution_clock;

long get_rss_kb(){
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss;
}

int main(int argc, char** argv){
    string mode="thread";
    int workers=4;
    long N=1000000;
    for(int i=1;i<argc;i++){
        string s=argv[i];
        if(s=="--mode") mode=argv[++i];
        else if(s=="--workers") workers=stoi(argv[++i]);
        else if(s=="--size") N=stol(argv[++i]);
    }
    cout<<"Mode: "<<mode<<", workers="<<workers<<", total items="<<N<<"\n";
    // prepare data
    vector<int> data;
    data.reserve(N);
    mt19937_64 rng(999);
    for(long i=0;i<N;i++) data.push_back((int)(rng() & 0x7fffffff));

    // let one integer shared buffer store current global max
    // thread mode, use atomic<int>
    // proc mode, use POSIX shared memory integer + sem for mutual exclusion

    auto map_start = clk::now();

    if(mode=="thread"){
        atomic<int> global_max;
        global_max.store(INT_MIN);
        vector<thread> thr;
        for(int w=0; w<workers; ++w){
            long l = w * N / workers;
            long r = (w+1) * N / workers;
            thr.emplace_back([l,r,&data,&global_max](){
                int local_max = INT_MIN;
                for(long i=l;i<r;i++) if(data[i] > local_max) local_max = data[i];
                // try to update global_max using compare_exchange loop
                int cur = global_max.load();
                while(cur < local_max && !global_max.compare_exchange_weak(cur, local_max)){
                    // cur updated to new value; loop if it's still < local_max
                }
            });
        }
        for(auto &t: thr) t.join();
        auto map_end = clk::now();
        int final_max = global_max.load();
        auto map_ms = chrono::duration_cast<chrono::milliseconds>(map_end - map_start).count();
        cout<<"Map time (ms): "<<map_ms<<"\n";
        cout<<"Final max: "<<final_max<<"\n";
        cout<<"Peak RSS (KB): "<<get_rss_kb()<<"\n";
    } else {
        // process mode
        // create shared int via mmap
        int *shared_max = (int*)mmap(nullptr, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(shared_max==MAP_FAILED){ perror("mmap"); return 1; }
        *shared_max = INT_MIN;
        // create a named semaphore for synchronization
        sem_t *sem = sem_open("/mapred_sem_example", O_CREAT | O_EXCL, 0600, 1);
        if(sem == SEM_FAILED){
            // try unlink then open
            sem_unlink("/mapred_sem_example");
            sem = sem_open("/mapred_sem_example", O_CREAT | O_EXCL, 0600, 1);
            if(sem == SEM_FAILED){ perror("sem_open"); return 1; }
        }

        // spawn children
        for(int w=0; w<workers; ++w){
            pid_t pid = fork();
            if(pid==0){
                long l = w * N / workers;
                long r = (w+1) * N / workers;
                int local_max = INT_MIN;
                for(long i=l;i<r;i++) if(data[i] > local_max) local_max = data[i];
                // attempt to update shared_max with semaphore for mutual exclusion
                sem_wait(sem);
                if(*shared_max < local_max) *shared_max = local_max;
                sem_post(sem);
                _exit(0);
            }
        }
        // parent waits
        for(int i=0;i<workers;i++) wait(nullptr);
        auto map_end = clk::now();
        int final_max = *shared_max;
        auto map_ms = chrono::duration_cast<chrono::milliseconds>(map_end - map_start).count();
        cout<<"Map time (ms): "<<map_ms<<"\n";
        cout<<"Final max: "<<final_max<<"\n";
        cout<<"Peak RSS (KB): "<<get_rss_kb()<<"\n";

        // cleanup
        sem_close(sem);
        sem_unlink("/mapred_sem_example");
        munmap(shared_max, sizeof(int));
    }
    return 0;
}
