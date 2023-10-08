#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <sstream>

struct Job {
    int curPriority, ticksLeft, allotLeft, startTime, runTime, timeLeft, firstRun, endTime;
    bool doingIO;
};

int seed = 0;
int numQueues = 3;
int quantum = 10;
int allotment = 5;
int numJobs = 3;
int maxlen = 120;
int boost = 100;

std::string jlist = "";
std::vector<int> quantumList;
std::vector<int> allotmentList;

int hiQueue = 0;
int finishedJobs = 0;

std::vector<Job> jobs;
std::vector<std::queue<int> > mlfqQueues;

void split(const std::string &s, char delimiter, std::vector<int> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        elems.push_back(std::stoi(item));
    }
}

int findQueue() {
    int q = hiQueue;
    while (q > 0) {
        if (!mlfqQueues[q].empty()) {
            return q;
        }
        q--;
    }
    if (!mlfqQueues[0].empty()) {
        return 0;
    }
    return -1;
}

void Abort(const std::string& str) {
    std::cerr << str << std::endl;
    exit(1);
}

void parseArguments(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "s:n:q:a:Q:A:j:m:M:B:i:S:I:l:c")) != -1) {
        switch (opt) {
            case 's':
                seed = std::stoi(optarg);
                break;
            case 'n':
                numQueues = std::stoi(optarg);
                break;
            case 'q':
                quantum = std::stoi(optarg);
                break;
            case 'a':
                allotment = std::stoi(optarg);
                break;
            case 'Q':
                split(optarg, ',', quantumList);
                break;
            case 'A':
                split(optarg, ',', allotmentList);
                break;
            case 'j':
                jlist = optarg;
                break;
            default:
                std::cerr << "Unknown option provided." << std::endl;
                exit(1);
        }
    }
}

void setupJobs() {
    if (!jlist.empty()) {
        std::stringstream ss(jlist);
        std::string item;
        while (std::getline(ss, item, ':')) {
            std::vector<int> jobInfo;
            split(item, ',', jobInfo);
            if (jobInfo.size() != 2) {
                std::cerr << "Badly formatted job string." << std::endl;
                exit(1);
            }
            Job newJob = {hiQueue, quantumList[hiQueue], allotmentList[hiQueue], 
                          jobInfo[0], jobInfo[1], jobInfo[1], -1, -1, false};
            jobs.push_back(newJob);
            mlfqQueues[0].push(jobs.size() - 1);
        }
    } else {
        for (int j = 0; j < numJobs; j++) {
            int startTime = 0;
            int runTime = rand() % (maxlen - 1) + 1;
            Job newJob = {hiQueue, quantumList[hiQueue], allotmentList[hiQueue], 
                          startTime, runTime, runTime, -1, -1, false};
            jobs.push_back(newJob);
            mlfqQueues[0].push(jobs.size() - 1);
        }
    }
}

void computeStatistics() {
    int totalJobs = jobs.size();
    int responseSum = 0;
    int turnaroundSum = 0;

    std::cout << "\nFinal statistics:\n";

    for (int i = 0; i < totalJobs; i++) {
        Job& jobRef = jobs[i];
        int response = jobRef.firstRun - jobRef.startTime;
        int turnaround = jobRef.endTime - jobRef.startTime;

        std::cout << "  Job " << i << ": startTime " << jobRef.startTime 
                  << " - response " << response 
                  << " - turnaround " << turnaround << std::endl;

        responseSum += response;
        turnaroundSum += turnaround;
    }

    std::cout << "\n  Avg: startTime n/a - response " 
              << static_cast<float>(responseSum) / totalJobs 
              << " - turnaround " 
              << static_cast<float>(turnaroundSum) / totalJobs << "\n\n";
}

void handlePriorityBoost(int currTime) {
    if (boost > 0 && currTime != 0 && currTime % boost == 0) {
        std::cout << "[ time " << currTime << " ] BOOST ( every " << boost << " )" << std::endl;
        for (int q = 0; q < numQueues - 1; q++) {
            while (!mlfqQueues[q].empty()) {
                int jobId = mlfqQueues[q].front();
                mlfqQueues[hiQueue].push(jobId);
                mlfqQueues[q].pop();
            }
        }
        for (Job &job : jobs) {
            if (job.timeLeft > 0) {
                job.curPriority = hiQueue;
                job.ticksLeft = quantumList[hiQueue];
                job.allotLeft = allotmentList[hiQueue];
            }
        }
    }
}

void runJob(int currQueue, int currTime) {
    int currJob = mlfqQueues[currQueue].front();
    mlfqQueues[currQueue].pop(); 

    Job& jobRef = jobs[currJob];



    if (jobRef.firstRun == -1) {
        jobRef.firstRun = currTime;
    }

    jobRef.timeLeft--;
    jobRef.ticksLeft--;

    std::cout << "[ time " << currTime << " ] Run JOB " << currJob << " at PRIORITY " << currQueue
              << " [ TICKS " << jobRef.ticksLeft << " ALLOT " << jobRef.allotLeft << " TIME "
              << jobRef.timeLeft << " (of " << jobRef.runTime << ") ]" << std::endl;

    if (jobRef.timeLeft == 0) {
        std::cout << "[ time " << currTime + 1 << " ] FINISHED JOB " << currJob << std::endl;
        jobRef.endTime = currTime + 1;
        finishedJobs++;  
        return;
    }

    if (jobRef.ticksLeft == 0) {
        if (jobRef.allotLeft > 1 || currQueue == 0) {
            jobRef.ticksLeft = quantumList[currQueue];
            jobRef.allotLeft--;
            mlfqQueues[currQueue].push(currJob); 
        } else if (jobRef.allotLeft == 1 && currQueue > 0) {
            currQueue--;
            jobRef.curPriority = currQueue;
            jobRef.ticksLeft = quantumList[currQueue];
            jobRef.allotLeft = allotmentList[currQueue];
            mlfqQueues[currQueue].push(currJob);
        }
    } else {
        mlfqQueues[currQueue].push(currJob);  
    }
}

void runSimulation() {
    int currTime = 0;
    int totalJobs = jobs.size();

    while (finishedJobs < totalJobs) {
        handlePriorityBoost(currTime);

        int currQueue = findQueue();
        if (currQueue == -1) {
            std::cout << "[ time " << currTime << " ] IDLE" << std::endl;
            currTime++;
            continue;
        }

        runJob(currQueue, currTime);
        currTime++;
    }

    computeStatistics();
}

int main(int argc, char* argv[]) {
    parseArguments(argc, argv);
    
    hiQueue = numQueues - 1;
    srand(seed);
    
    if (quantumList.empty()) {
      for (int i = 0; i < numQueues; i++) {
          quantumList.push_back(quantum);
      }
    }

    if (allotmentList.empty()) {
        for (int i = 0; i < numQueues; i++) {
            allotmentList.push_back(allotment);
        }
    }

    mlfqQueues.resize(numQueues);

    setupJobs();

    runSimulation();

    return 0;
}
