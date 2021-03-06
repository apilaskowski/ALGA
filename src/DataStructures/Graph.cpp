/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Graph.cpp
 * Author: sylwester
 * 
 * Created on November 17, 2018, 3:58 PM
 */

#include <DataStructures/Graph.h>
#include <Utils/MyUtils.h>
#include <Params.h>
#include <Global.h>
#include <AlignmentControllers/AlignmentController.h>
#include <thread>
#include <DataStructures/FAU.h>
#include <future>

#include "DataStructures/Graph.h"

Graph::Graph(int N) : edges(0) {

    V = VVPII(N);
//    contractedEdges = VMILPII(N);
    mutexes = new vector<mutex>(ceil((double) N / MUTEX_SCALE));

}

Graph::Graph(const Graph &orig) {
    V = orig.V;
    edges = orig.edges;
    contractedEdges = orig.contractedEdges;
    this->mutexes = new vector<mutex>(ceil((double) V.size() / MUTEX_SCALE));
}

Graph::~Graph() {
    clear();
}

void Graph::write() {
    cout << "size = " << size() << endl;
    for (int i = 0; i < size(); i++) {
        cout << i << ":\t";
        for (int k = 0; k < V[i].size(); k++) cout << V[i][k].first << " ";
        cout << endl;
    }
}

void Graph::addDirectedEdge(int a, int b, int offset) {
    if (a == b) return;

    VPII::iterator it = V[a].end();
    for (it = V[a].begin(); it != V[a].end(); ++it) if (it->first == b) break;
    if (it == V[a].end()) {
        V[a].push_back({b, offset});
        edges++;
    } else {
        int ind = it - V[a].begin();

        if (ind < 0 || ind >= V[a].size()) {
            cerr << "in addDirectedEdge , ind = " << ind << "  V[a].size() = " << V[a].size() << endl;
            exit(1);
        }
        if (offset < V[a][ind].second) V[a][ind].second = offset;
    }

}

void Graph::pushDirectedEdge(int a, int b, int offset) {
    V[a].emplace_back(b, offset);
}


void Graph::mergeVertices(int a, int b, int offset) {

    for (int i = 0; i < V[a].size(); i++) {
        if (V[a][i].first != b && V[a][i].first != a)
            addDirectedEdge(b, V[a][i].first, V[a][i].second); // i do not want to add edge (b,b)
    }


    removeDirectedEdge(b, a);

    VPII().swap(V[a]);
//    contractedEdges[a].clear();
    if (!contractedEdges.empty() && contractedEdges[a] != nullptr) contractedEdges[a]->clear();

    addDirectedEdge(a, b, offset);
}


bool Graph::removeDirectedEdge(int a, int b) {
//    if( !contractedEdges.empty() ) contractedEdges[a].erase(b);
    if (!contractedEdges.empty()) {
//        contractedEdges[a]->erase(b);
//        auto it = findContractedEdge(a, b);
        removeContractedEdge(a, b);
    }

    bool removed = false;
    int p = (int) V[a].size() - 1;
    for (int i = (int) V[a].size() - 1; i >= 0; i--) {
        if (V[a][i].first == b) {
            swap(V[a][i], V[a][p]);

            V[a].pop_back();
            p--;
            removed = true;
        }
    }

//    if (V[a].empty()) VPII().swap(V[a]); // clear space of empty vector - will it or not affect memory peak?

    return removed;
}


int Graph::getWeight(int i, int k) {
    return V[i][k].second;
}

VI *Graph::getInDegrees() {
    VI *inDeg = new VI(size(), 0);

    vector<std::future<void> > futures(Params::THREADS - 1);

    auto worker = [=, &inDeg](int a, int b) {
        for (int j = a; j <= b; j++) {
            for (auto p : V[j]) {
                int d = p.first;
                lockNode(d);
                (*inDeg)[d]++;
                unlockNode(d);
            }
        }
    };

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        futures[i - 1] = std::async(std::launch::async, worker, a, b);
    }

    worker(0, W - 1);
    for (auto &p : futures) p.get();


    return inDeg;
}

ostream &operator<<(ostream &str, Graph &G) {
    for (int i = 0; i < G.size(); i++) {
        str << i << ": ";
        for (int k = 0; k < G[i].size(); k++) str << setw(7) << G[i][k].first << "(" << G[i][k].second << ") ";
        str << endl;
    }
    return str;
}

bool Graph::containsEdge(int a, int b) {

    VPII::iterator it = V[a].end();
    for (it = V[a].begin(); it != V[a].end(); ++it) if (it->first == b) break;

    if (it != V[a].end())return true;
    else return false;
}

bool Graph::containsEdgeShorterOrEqual(int a, int b, int offset) {

    for (int i = 0; i < (int) V[a].size(); i++) {
        if (V[a][i].first == b && V[a][i].second <= offset) return true;
    }
    return false;
}

void Graph::writeAllEdgesWithNode(int id) {

    cerr << "All edges for node " << id << endl << "in nodes:" << endl;
    for (int i = 0; i < size(); i++) {
        for (int k = 0; k < V[i].size(); k++) if (V[i][k].first == id) cerr << i << "(" << V[i][k].second << ") ";
    }
    cerr << endl;
    cerr << "out nodes:" << endl;
    for (int k = 0; k < V[id].size(); k++) {
        cerr << V[id][k].first << "(" << V[id][k].second << ") ";
    }
    cerr << endl;
}

void Graph::clearNode(int v) {
    clearNeighborsForNode(v);
}

void Graph::clearNeighborsForNode(int v) {
    VPII().swap(V[v]);

    if (!contractedEdges.empty()) {
        delete contractedEdges[v];
        contractedEdges[v] = nullptr;
    }

}

void Graph::write(int a, int b) {
    for (int i = a; i <= b; i++) {
        cerr << i << ": ";
        for (int k = 0; k < V[i].size(); k++) cerr << V[i][k].first << "(" << V[i][k].second << ") ";
        cerr << endl;
    }
}


bool Graph::deserializeGraph(string fileName) {

    ifstream str(fileName, ios::binary | ios::in);
    if (str.good() == false) {
        str.close();
        return false;
    }

    unsigned s;
    str.read((char *) &s, sizeof(s));


    V = VVPII(s);
//    contractedEdges = VMILPII(s);
    if (mutexes != nullptr) {
        delete mutexes;
        mutexes = nullptr;
    }
    mutexes = new vector<mutex>(ceil((double) s / LOG2_MUTEX_SCALE));

    cerr << endl;
    int progressCounter = 0;
    for (int i = 0; i < s; i++) {
        int id;
        str.read((char *) &id, sizeof(id)); // this is id of given vertex

        int t; // number of neighbors
        str.read((char *) &t, sizeof(t));

        V[id].reserve(t);

        for (int k = 0; k < t; k++) {
            int d;
            int w;
            str.read((char *) &d, sizeof(d));
            str.read((char *) &w, sizeof(w));

            V[id].push_back({d, w});
        }

        MyUtils::writeProgress(i, s, progressCounter, "deserializing graph", 1);
    }
    cerr << endl;

    str.close();

    return true;
}

void Graph::serializeGraph(string fileName) {
    ofstream str(fileName, ios::binary | ios::out);

    unsigned s = size();
    str.write((char *) &s, sizeof(s));

    int progressCounter = 0;
    cerr << endl;
    for (int i = 0; i < s; i++) {
        str.write((char *) &i, sizeof(i));

        vector<pair<int, int>> neigh = getNeighbors(i);

        int t = (int) neigh.size();
        str.write((char *) &t, sizeof(t));
        for (auto &a : neigh) {
            int d = a.first;
            int w = a.second;
            str.write((char *) &d, sizeof(d));
            str.write((char *) &w, sizeof(w));
        }

        MyUtils::writeProgress(i, s, progressCounter, "serializing graph", 1);
    }

    str.close();

    cerr << "Graph serialized!" << endl;
}

vector<pair<int, int>> Graph::getNeighbors(int id) {
    vector<pair<int, int> > neigh;
    neigh = V[id];
//    return std::move(neigh);
    return neigh;
}


void Graph::addCompRevConnection(Read *r1, Read *r2, int offset) {
    Read *r1revcomp = Global::READS[r1->getIdOfCompRevRead()];
    Read *r2revcomp = Global::READS[r2->getIdOfCompRevRead()];
    int rightOffset = Read::getRightOffset(r1, r2, offset);

    addDirectedEdge(r2revcomp->getId(), r1revcomp->getId(), rightOffset);
}

bool Graph::operator==(Graph &oth) {
    if (size() != oth.size()) return false;
    for (int i = 0; i < size(); i++) {
        vector<pair<int, int>> neigh = getNeighbors(i);
        vector<pair<int, int>> othNeigh = oth.getNeighbors(i);

        if (!neigh.empty()) sort(neigh.begin(), neigh.end());
        if (!othNeigh.empty()) sort(othNeigh.begin(), othNeigh.end());

        while (neigh.size() > 0 && neigh[0].second == 0) neigh.erase(neigh.begin());
        while (othNeigh.size() > 0 && othNeigh[0].second == 0) othNeigh.erase(othNeigh.begin());

        if (neigh.size() != othNeigh.size()) {
            cerr << endl << "i = " << i << endl;
            cerr << "sizes: " << neigh.size() << ", " << othNeigh.size() << endl;
            for (int k = 0; k < neigh.size(); k++) {
                cerr << "neigh[k] = (" << neigh[k].first << "," << neigh[k].second << ")   othNeigh[k] = ("
                     << othNeigh[k].first << "," << othNeigh[k].second << ")" << endl;
            }
            return false;
        }
        for (int k = 0; k < neigh.size(); k++) {
            if (neigh[k].first != othNeigh[k].first || neigh[k].second != othNeigh[k].second) {
                cerr << "neigh[k] = (" << neigh[k].first << "," << neigh[k].second << ")   othNeigh[k] = ("
                     << othNeigh[k].first << "," << othNeigh[k].second << ")" << endl;
                return false;
            }
        }
    }

    return true;
}

void Graph::retainOnlySmallestOffset() {
    vector<std::thread> parallelJobs;
    parallelJobs.reserve(Params::THREADS);

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        parallelJobs.push_back(thread([=] { retainOnlySmallestOffsetJob(a, b, i); }));
    }

    retainOnlySmallestOffsetJob(0, W - 1, 0);

    for (auto &p : parallelJobs) p.join();
    cerr << endl;
}


void Graph::retainOnlySmallestOffsetJob(int a, int b, int thread_id) {
    VPII newNeigh;
    int progressCounter = 0;
    for (int i = a; i <= b; i++) {
        sort(V[i].begin(), V[i].end());
        newNeigh.reserve(V[i].size());
        int p = 0;
        while (p < V[i].size()) {
            newNeigh.push_back(V[i][p]);
            p++;

            while (p < V[i].size() && V[i][p - 1].first == V[i][p].first) p++;

        }
        swap(V[i], newNeigh);
        newNeigh.clear();
        if (thread_id == 0)
            MyUtils::writeProgress(i - a + 1, b - a + 1, progressCounter,
                                   "retaining smallest offset for same edge in thread 0", 1);
    }
}


bool Graph::contractPath(int a, int b, int c) {
    if (a == c) return false;
    if (V[b].size() != 1) return false;
    if (containsEdge(a, b) == false) return false;


    int wbc = V[b][0].second;
    int wab = findWeight(a, b);
    int wabc = wab + wbc;

    int EDGE_LENGTH_THRESHOLD = Params::MAX_OFFSET_PARALLEL_PATHS;

    bool existsEdgeAC = containsEdge(a, c);
    if (existsEdgeAC && wabc >= EDGE_LENGTH_THRESHOLD) return false; // ORIGINAL
//    if (existsEdgeAC && findWeight(a,c) != wab + wbc) return false;
    /*if( existsEdgeAC ){ // ELT_LONGER - keep only the longer edge
        if( findWeight(a,c) <= wab + wbc ) removeDirectedEdge(a,c); // keep the longer edge
        else if( findWeight(a,c) > wab + wbc ){
            removeDirectedEdge(a,b);
            removeDirectedEdge(b,c);
            return true;
        }
        else return false;
    }*/

    bool existsLongEdgeAC = containsEdgeLongerOrEqual(a, c, EDGE_LENGTH_THRESHOLD);
    if (existsLongEdgeAC) return false;

    if (contractedEdges[a] == nullptr) contractedEdges[a] = new VILPII();
    if (contractedEdges[b] == nullptr) contractedEdges[b] = new VILPII();
//    if (contractedEdges[c] == nullptr) contractedEdges[c] = new VILPII();

    if (contractedEdges[b]->size() == 0) {
        LPII l(1, PII(c, wbc));
        addContractedEdgeOrReplace(b, c, l);
    }

    if (findContractedEdge(a, b) == contractedEdges[a]->end()) {
        LPII l(1, PII(b, wab));
        addContractedEdgeOrReplace(a, b, l);
    }

    // if the resulted path is 'short' then i add it (now matter if other short path already exists)
    if (wabc < EDGE_LENGTH_THRESHOLD || existsLongEdgeAC == false) {

        removeDirectedEdge(a, c);
        // then i assume the short path is unique so i can replace the old one if it exists. First step - new one is empty.
        addContractedEdgeOrReplace(a, c, LPII());

        auto it = findContractedEdge(a, c);
        if (it == contractedEdges[a]->end()) addContractedEdgeOrReplace(a, c, LPII());
        LPII *l;

        {
            // apppending edge a->b to the list.
            l = &findContractedEdge(a, c)->second; // pointer to the list where we will splice
            it = findContractedEdge(a, b);
            l->splice(l->end(), it->second);
        }

        {
            // appending edge b->c to the list.
            l = &findContractedEdge(a, c)->second;
            it = findContractedEdge(b, c);
            l->splice(l->end(), it->second);
        }

        removeDirectedEdge(a, b);
        clearNode(b);

        addDirectedEdge(a, c, wabc);



        return true;
    }

    return false;

}

int Graph::findWeight(int a, int b) {
    for (auto p : V[a]) {
        if (p.first == b) return p.second;
    }
    return -1;
}

bool Graph::containsEdgeLongerOrEqual(int a, int b, int offset) {

    for (int i = 0; i < (int) V[a].size(); i++) {
        if (V[a][i].first == b && V[a][i].second >= offset) return true;
    }
    return false;
}

LPII Graph::getContractedEdgePath(int a, int b) {

    if (contractedEdges[a] == nullptr || findContractedEdge(a, b) == contractedEdges[a]->end()) {
        if (containsEdge(a, b)) {
            return LPII(1, PII(b, findWeight(a, b)));
        } else {
            return LPII();
        }
    }

    return findContractedEdge(a, b)->second;
}


Graph Graph::getReverseGraph() {
    Graph GRev(size());

    vector<std::thread> parallelJobs;
    parallelJobs.reserve(Params::THREADS);

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        parallelJobs.push_back(thread([=, &GRev] { getReverseGraphJob(a, b, i, GRev); }));
    }

    getReverseGraphJob(0, W - 1, 0, GRev);

    for (auto &p : parallelJobs) p.join();
    cerr << endl;

    return GRev;
}


void Graph::getReverseGraphJob(int a, int b, int thread_id, Graph &GRev) {
    int progressCounter = 0;
    for (int i = a; i <= b; i++) {
        VPII neigh = getNeighbors(i);
        for (auto p : neigh) {
            GRev.lockNode(p.first);
            GRev.pushDirectedEdge(p.first, i, p.second);
            GRev.unlockNode(p.first);
        }
        if (thread_id == 0)
            MyUtils::writeProgress(i + 1, size() / Params::THREADS, progressCounter, "creating reverse graph", 1);
    }
}


void Graph::writeNonisolatedNodes(int a, int b) {
    VI *indeg = getInDegrees();
    for (int i = a; i <= min(b, (int) size() - 1); i++) {
        VPII neigh = getNeighbors(i);
        if ((*indeg)[i] > 0 || neigh.size() > 0) {
            cerr << i << ": ";
            for (auto a : neigh) cerr << a.first << "(" << a.second << ")" << " ";
            cerr << endl;
        }
    }
    delete indeg;
    indeg = nullptr;
}

void Graph::writeContractedPath(int a, int b) {
    LPII path = getContractedEdgePath(a, b);

    int length = 0;
    cerr << a;
    for (auto p : path) {
        cerr << " -> " << p.first << "(" << p.second << ")";
        length += p.second;
    }
    cerr << endl << "sum of offset on the path: " << length << endl;

}

bool Graph::operator<(Graph &oth) {
    if (size() != oth.size()) return false;

    int progressCounter = 0;
    for (int i = 0; i < size(); i++) {
        VPII neigh = getNeighbors(i);
        if (V[i].size() > oth.V[i].size()) return false;

        for (auto p : neigh) {
            if (!oth.containsEdge(i, p.first) || oth.findWeight(i, p.first) != p.second) {
                return false;
            }
        }
        MyUtils::writeProgress(i + 1, size(), progressCounter, "checking < operator in Graph", 1);
    }

    return true;
}

void Graph::sortEdgesByIncreasingOffset() {
    vector<std::thread> parallelJobs;
    parallelJobs.reserve(Params::THREADS);

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        parallelJobs.push_back(thread([=] { sortEdgesByIncreasingOffsetJob(a, b, i); }));
    }

    sortEdgesByIncreasingOffsetJob(0, W - 1, 0);

    for (auto &p : parallelJobs) p.join();
    cerr << endl;

}

void Graph::sortEdgesByIncreasingOffsetJob(int a, int b, int thread_id) {
    int progressCounter = 0;
    for (int i = a; i <= b; i++) {
        sort(V[i].begin(), V[i].end(), [](auto &p, auto &q) {
            if (p.second != q.second) return p.second < q.second;
            else return p.first < q.first;
        });
        if (thread_id == 0)
            MyUtils::writeProgress(i + 1, size() / Params::THREADS, progressCounter, "sorting edges by minimal offset",
                                   1);
    }
}

void Graph::operator=(const Graph &orig) {
    V = orig.V;


    edges = orig.edges;
    contractedEdges = orig.contractedEdges;
    delete mutexes;
    mutexes = 0;
    this->mutexes = new vector<mutex>(ceil((double) V.size() / MUTEX_SCALE));

}

void Graph::push_node() {

    delete mutexes;
    mutexes = new vector<mutex>(ceil((double) (size() + 1) / MUTEX_SCALE));
    contractedEdges.resize(size() + 1);
    V.resize(size() + 1);
}


void Graph::clear() {
    VVPII().swap(V);

    for (int i = 0; i < contractedEdges.size(); i++) {
        if (contractedEdges[i] != nullptr) {
            delete contractedEdges[i];
            contractedEdges[i] = nullptr;
        }
    }
    contractedEdges.clear();
    if (mutexes != nullptr) {
        delete mutexes;
        mutexes = nullptr;
    }

}

LL Graph::countEdges() {

    vector<std::future<long long> > futures(Params::THREADS - 1);

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        futures[i - 1] = std::async(std::launch::async, [=](int a, int b, int i) { return countEdgesJob(a, b, i); }, a,
                                    b, i);
    }

    LL res = countEdgesJob(0, W - 1, 0);

    for (auto &p : futures) {
        res += p.get();
    }

    return res;

}

LL Graph::countEdgesJob(int a, int b, int thread_id) {
    LL res = 0;
    for (int i = a; i <= b; i++) res += V[i].size();
    return res;
}


void Graph::pruneGraph() {

    vector<std::future<void> > futures(Params::THREADS - 1);

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        futures[i - 1] = std::async(std::launch::async, [=](int a, int b) {
            for (int i = a; i <= b; i++) {
                VPII(V[i]).swap(V[i]);
            }
        }, a, b);
    }

    for (int i = 0; i <= W - 1; i++) VPII(V[i]).swap(V[i]);


    for (auto &p : futures) {
        p.get();
    }

}

void Graph::reverseGraph() {
    /**
     * if true, then i reverse the graph in-place - we do not create a copy of VVPII(size()) that is memory-consuming.
     * It may be a bit slower however due to a bit more locks added.
     */
    const bool usedImprovedRevGraph = false;
    if (usedImprovedRevGraph) {
        vector<unsigned> degs(size(), 0); // will the unsigned short be enough??
        auto fun1 = [=, &degs](unsigned a, unsigned b) {
            for (unsigned i = a; i <= b; i++) degs[i] = V[i].size();
        };
        vector<std::future<void> > futures(Params::THREADS - 1);
        int W = (int) ceil((double) size() / Params::THREADS);
        for (int i = 1; i < Params::THREADS; i++) {
            int a = i * W;
            int b = min((i + 1) * W - 1, (int) size() - 1);
            futures[i - 1] = std::async(std::launch::async, fun1, a, b);
        }
        fun1(0, W - 1);
        for (auto &p : futures) p.get();
        // degrees initialized

        auto worker = [=, &degs](int a, int b) {
            for (int j = a; j <= b; j++) {
                lockNode(j);
                VPII neigh = V[j]; // accessing the neighborhood, not to block two nodes in the same time
                unlockNode(j);
                for (unsigned i = 0; i < degs[j]; i++) {
                    PII &p = neigh[i];
                    int d = p.first;
                    int off = p.second;
                    lockNode(d);
                    V[d].emplace_back(j, off);
                    unlockNode(d);
                }
                lockNode(j);
                V[j].erase(V[j].begin(), V[j].begin() + degs[j]);
                VPII(V[j]).swap(V[j]);
                unlockNode(j);
            }
        };

        for (int i = 1; i < Params::THREADS; i++) {
            int a = i * W;
            int b = min((i + 1) * W - 1, (int) size() - 1);
            futures[i - 1] = std::async(std::launch::async, worker, a, b);
        }
        worker(0, W - 1);
        for (auto &p : futures) p.get();


        return;
    } else {

        VVPII rev(size());

        const bool useIndegInitialization = false;
        if (useIndegInitialization) {
            VI *inDegrees = getInDegrees();
            for (int i = 0; i < size(); i++) rev[i].reserve((*inDegrees)[i]);
            delete inDegrees;
            inDegrees = nullptr;
        }

        vector<std::future<void> > futures(Params::THREADS - 1);

        auto worker = [=, &rev](int a, int b) {
            for (int j = a; j <= b; j++) {
                for (auto p : V[j]) {
                    int d = p.first;
                    int off = p.second;
                    lockNode(d);
                    rev[d].emplace_back(j, off);
                    unlockNode(d);
                }

                VPII().swap(V[j]);
            }
        };

        int W = (int) ceil((double) size() / Params::THREADS);
        for (int i = 1; i < Params::THREADS; i++) {
            int a = i * W;
            int b = min((i + 1) * W - 1, (int) size() - 1);
            futures[i - 1] = std::async(std::launch::async, worker, a, b);
        }
        worker(0, W - 1);
        for (auto &p : futures) p.get();


        swap(V, rev);
    }

}

void Graph::createContractedEdgesVector() {
    contractedEdges = VVILPII(size(), nullptr);
    return; // #TEST
    VB indeg = hasPositiveIndegree();

    for (int i = 0; i < size(); i++) {
        if (V[i].size() > 0 || indeg[i]) {
            contractedEdges[i] = new VILPII();
        }
    }
}

VVPII Graph::getReverseGraphNeighborhoods() {
    VVPII rev(size());

    const bool useIndegInitialization = false;
    if (useIndegInitialization) {
        VI *indeg = getInDegrees();
        for (int i = 0; i < size(); i++) rev[i].reserve((*indeg)[i]);
        delete indeg;
        indeg = nullptr;
    }

    vector<std::future<void> > futures(Params::THREADS - 1);

    auto worker = [=, &rev](int a, int b) {
        for (int j = a; j <= b; j++) {
            for (auto p : V[j]) {
                int d = p.first;
                int off = p.second;
                lockNode(d);
                rev[d].emplace_back(j, off);
                unlockNode(d);
            }
        }
    };

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);
        futures[i - 1] = std::async(std::launch::async, worker, a, b);
    }
    worker(0, W - 1);
    for (auto &p : futures) p.get();

    if (!useIndegInitialization) {
        auto worker2 = [=, &rev](int a, int b) {
            for (int j = a; j <= b; j++) {
                VPII(rev[j]).swap(rev[j]);
            }
        };

        for (int i = 1; i < Params::THREADS; i++) {
            int a = i * W;
            int b = min((i + 1) * W - 1, (int) size() - 1);
            futures[i - 1] = std::async(std::launch::async, worker2, a, b);
        }
        worker2(0, W - 1);
        for (auto &p : futures) p.get();
    }

    return rev;
}

VB Graph::hasPositiveIndegree() {
    VB inDeg(size());
    vector<std::future<void> > futures(Params::THREADS - 1);

    auto worker = [=, &inDeg](int a, int b) {
        for (int j = a; j <= b; j++) {
            for (auto p : V[j]) {
                int d = p.first;
                lockNode(d);
                inDeg[d] = true;
                unlockNode(d);
            }
        }
    };

    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);

        futures[i - 1] = std::async(std::launch::async, worker, a, b);
    }

    worker(0, W - 1);
    for (auto &p : futures) p.get();

    return inDeg;
}

bool Graph::removeContractedEdge(unsigned int a, unsigned int b) {
    if (contractedEdges[a] == nullptr) return false;
    auto it = findContractedEdge(a, b);
    if (it != contractedEdges[a]->end()) {
        contractedEdges[a]->erase(it);
        return true;
    }
    return false;
}

VILPII::iterator Graph::findContractedEdge(unsigned a, unsigned b) {
    assert(contractedEdges[a] != nullptr);
    for (auto it = contractedEdges[a]->begin(); it != contractedEdges[a]->end(); ++it) {
        if (it->first == b) return it;
    }
    return contractedEdges[a]->end();
}

void Graph::addContractedEdgeOrReplace(unsigned int a, unsigned int b, LPII e) {
    assert(contractedEdges[a] != nullptr);
    auto it = findContractedEdge(a, b);
    if (it == contractedEdges[a]->end()) {
        contractedEdges[a]->emplace_back(b, std::move(e));
    } else {
        it->second = e;
    }
}

void Graph::reverseGraphInPlace() {
    vector<unsigned> degs(size(), 0); // will the unsigned short be enough??
    auto fun1 = [=, &degs](unsigned a, unsigned b) {
        for (unsigned i = a; i <= b; i++) degs[i] = V[i].size();
    };
    vector<std::future<void> > futures(Params::THREADS - 1);
    int W = (int) ceil((double) size() / Params::THREADS);
    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);
        futures[i - 1] = std::async(std::launch::async, fun1, a, b);
    }
    fun1(0, W - 1);
    for (auto &p : futures) p.get();
    // degrees initialized

    auto worker = [=, &degs](int a, int b) {
        for (int j = a; j <= b; j++) {
            lockNode(j);
            VPII neigh = V[j]; // accessing the neighborhood, not to block two nodes in the same time
            unlockNode(j);
            for (unsigned i = 0; i < degs[j]; i++) {
                PII &p = neigh[i];
                int d = p.first;
                int off = p.second;
                lockNode(d);
                V[d].emplace_back(j, off);
                unlockNode(d);
            }
            lockNode(j);
            V[j].erase(V[j].begin(), V[j].begin() + degs[j]);
            VPII(V[j]).swap(V[j]);
            unlockNode(j);
        }
    };

    for (int i = 1; i < Params::THREADS; i++) {
        int a = i * W;
        int b = min((i + 1) * W - 1, (int) size() - 1);
        futures[i - 1] = std::async(std::launch::async, worker, a, b);
    }
    worker(0, W - 1);
    for (auto &p : futures) p.get();

    return;
}

void Graph::writeBasicStatistics() {
    return;
    cerr << "Basic graph statistics: " << endl;
    cerr << "Nodes: " << size() << endl;
    cerr << "Edges: " << countEdges() << endl;

    cerr << endl;
    cerr << "Maximum outdegree: "
         << max_element(V.begin(), V.end(), [](auto &v1, auto &v2) { return v1.size() < v2.size(); })->size() << endl;
    VI cnt(size(), 0);
    for (int i = 0; i < size(); i++) cnt[V[i].size()]++;
    int T = 5;
    long long S = 0;
    for (int i = 0; i <= T; i++) {
        cerr << "Nodes with outdegree " << i << ": " << cnt[i] << endl;
        S += cnt[i];
    }
    cerr << "Total nodes with outdegree <= " << T << ": " << S << endl;

    cerr << endl;
    S = 0;
    fill(cnt.begin(), cnt.end(), 0);
    VI *indegs = getInDegrees();
    cerr << "Maximum indegree: "
         << *max_element((*indegs).begin(), (*indegs).end(), [](auto &v1, auto &v2) { return v1 < v2; }) << endl;
    for (int i = 0; i < size(); i++) cnt[(*indegs)[i]]++;
    delete indegs;
    for (int i = 0; i <= T; i++) {
        cerr << "Nodes with indegree " << i << ": " << cnt[i] << endl;
        S += cnt[i];
    }
    cerr << "Total nodes with indegree <= " << T << ": " << S << endl;

    cerr << endl;
}


