// Copyright (c) 2013
// Author: Muye (muyepiaozhou@gmail.com)

#include "svdpp.h"
#include <cmath>
#include <ctime>
#include <cstdlib>

SVDPP::SVDPP(int dimension, int nu, int nm, int nf) : dim(dimension), mean(0.0) {
    customers.resize(nu+1);
    movies.resize(nm+1);
    fdbks.resize(nf+1);
}

SVDPP::~SVDPP() {}

void SVDPP::TrainDataLoad(const string& path) {
    int cid, mid, num = 0;
    double rate;
    mean = 0.0;
    ifstream ifs(path.data(), ifstream::in);
    if(!ifs.is_open()) {
        cout << path << " cann't be opened" << endl;
        exit(-1);
    }
    while(ifs >> cid >> mid >> rate) {
        ++num;
        mean += rate;
        ++movies[mid].n;
        customers[cid].rated.push_back(Entry(mid, rate));
    }
    ifs.close();
    mean /= num;
}

void SVDPP::ProbeDataLoad(const string& path) {
    ifstream ifs(path.data(), ifstream::in);
    int cid, mid;
    double rate;
    if(!ifs.is_open()) {
        cout << path << " cann't be opened" << endl;
        exit(-1);
    }
    while(ifs >> cid >> mid >> rate) {
        probes.push_back(ProbeEntry(cid, mid, rate));
    }
    ifs.close();
}

void SVDPP::ImplicitDataLoad(const string& path) {
    int cid, mid;
    ifstream ifs(path.data(), ifstream::in);
    if(!ifs.is_open()) {
        cout << path << " cann't be opened" << endl;
        exit(-1);
    }
    while(ifs >> cid >> mid) {
        customers[cid].imfdbk.push_back(mid);
    }
    ifs.close();
}

void SVDPP::Train(int maxloops, double alpha1, double alpha2, double beta1, double beta2, double beta3) {
    cout << "mean = " << mean << endl;
    cout << "initialize bais" << endl;
    InitBais();
    cout << "initialize P and Q" << endl;
    InitPQ();
    long double prmse = 100000000000.0, rmse = 0.0;
    for(int loop = 0; loop < maxloops; ++loop) {
        rmse = 0.0;
        for(vector<Movie>::iterator it = movies.begin(); it != movies.end(); ++it) {
            rmse += beta3 * pow(it->bi, 2);
            for(int i = 0; i < dim; ++i) rmse += beta3 * pow(it->qi[i], 2);
        }
        for(vector<Customer>::iterator it = ++customers.begin(); it != customers.end(); ++it) {
            rmse += pow(it->bu, 2);
            vector<double>& pu = it->pu;
            vector<int>& fd = it->imfdbk;
            vector<Entry>& rd = it->rated;
            vector<double> tmp(dim, 0.0), tmp1(dim, 0.0);
            for(vector<int>::iterator itf = fd.begin(); itf != fd.end(); ++itf) {
                vector<double>& yj = fdbks[*itf].yj;
                for(int i = 0; i < dim; ++i) {
                    tmp[i] += yj[i];
                    rmse += beta3 * pow(yj[i], 2);
                }
            }
            for(vector<Entry>::iterator itr = rd.begin(); itr != rd.end(); ++itr) {
                vector<double>& yi = movies[itr->mid].yi;
                for(int i = 0; i < dim; ++i) {
                    tmp1[i] += yi[i];
                    rmse += beta3 * pow(yi[i], 2);
                }
            }
            double ru = sqrt((double)fd.size()), ru1 = sqrt((double)rd.size());
            for(int i = 0; i < dim; ++i) {
                tmp[i] /= ru;
                tmp1[i] /= ru1;
                tmp[i] += tmp1[i];
                tmp[i] += pu[i];
                rmse += beta3 * pow(pu[i], 2);
            }
            for(vector<Entry>::iterator itd = rd.begin(); itd != rd.end(); ++itd) {
                double& bi = movies[itd->mid].bi;
                double rui = mean + it->bu + bi;
                vector<double>& qi = movies[itd->mid].qi;
                for(int i = 0; i < dim; ++i) {
                    rui += tmp[i] * qi[i];
                }
                double eui = itd->rate - rui;
                it->bu += alpha1 * (eui - beta1 * it->bu); // update bu
                bi += alpha1 * (eui - beta1 * bi); // update bi
                for(int i = 0; i < dim; ++i) pu[i] += alpha2 * (eui * qi[i] - beta2 * pu[i]); // update pu
                for(vector<int>::iterator itf = fd.begin(); itf != fd.end(); ++itf) {
                    vector<double>& yj = fdbks[*itf].yj;
                    // update yj
                    for(int i = 0; i < dim; ++i) yj[i] += alpha2 * (eui * qi[i] / ru - beta2 * yj[i]);
                }
                for(vector<Entry>::iterator itr = rd.begin(); itr != rd.end(); ++itr) {
                    vector<double>& yi = movies[itr->mid].yi;
                    // update yi
                    for(int i = 0; i < dim; ++i) yi[i] += alpha2 * (eui * qi[i] / ru1 - beta2 * yi[i]);
                }
                // update qi
                for(int i = 0; i < dim; ++i) qi[i] += alpha2 * (eui * tmp[i] - beta2 * qi[i]);
            }
        }
        if(loop > 3 && rmse > prmse) {
            cout << "Over! rmse = " << rmse << "\tcalerror : " << CalError() << endl;
            break;
        }
        cout << "loop[" << loop << "]\trmse = " << rmse << "\tcalerror : " << CalError() << endl;
        prmse = rmse;
    }
}

void SVDPP::Predict(const string& path, const string& result) {
    ifstream ifs(path.data(), ifstream::in);
    ofstream ofs(result.data(), ofstream::out);
    if(!ifs.is_open() || !ofs.is_open()) {
        cout << "cann't open file" << endl;
        exit(-1);
    }
    int custId, movieId;
    double rate;
    while(ifs >> custId >> movieId) {
        ofs << custId << '\t' << movieId << '\t';
        rate = predict(custId, movieId);
        if(rate < 1.0) rate = 1.0;
        if(rate > 5.0) rate = 5.0;
        ofs << rate << endl;
    }
    ifs.close();
    ofs.close();
}

void SVDPP::InitBais() {
    for(vector<Customer>::iterator it = ++customers.begin(); it != customers.end(); ++it) {
        for(vector<Entry>::iterator ie = it->rated.begin(); ie != it->rated.end(); ++ie) {
            it->bu += ie->rate - mean;
            movies[ie->mid].bi += ie->rate - mean;
        }
        it->bu /= it->rated.size();
    }
    for(vector<Movie>::iterator it = ++movies.begin(); it != movies.end(); ++it) it->bi /= it->n;
}

void SVDPP::InitPQ() {
    srand((unsigned)time(NULL));
    for(vector<Customer>::iterator itu = ++customers.begin(); itu != customers.end(); ++itu) SetRand(itu->pu);
    for(vector<Movie>::iterator itm = ++movies.begin(); itm != movies.end(); ++itm) {
        SetRand(itm->qi);
        SetRand(itm->yi);
    }
    for(vector<FeedBack>::iterator itf = ++fdbks.begin(); itf != fdbks.end(); ++itf) SetRand(itf->yj);
}

void SVDPP::SetRand(vector<double>& v) {
    v.clear();
    v.resize(dim, 0.0);
    double d = sqrt(dim);
    for(int i = 0; i < dim; ++i) v[i] = 0.1 * (rand() / (double)RAND_MAX) / d;
}

double SVDPP::CalError() {
    double rmse = 0.0, rate = 0.0;
    vector<ProbeEntry>::iterator it = probes.begin();
    for(; it != probes.end(); ++it) {
        rate = predict(it->cid, it->mid);
        rmse += pow(it->rate - rate, 2);
    }
    return sqrt(rmse/probes.size());
}

double SVDPP::predict(const int& uid, const int& iid) {
    vector<double>& pu = customers[uid].pu;
    vector<double>& qi = movies[iid].qi;
    vector<int>& fd = customers[uid].imfdbk;
    vector<Entry>& rated = customers[uid].rated;
    vector<double> tmp1(dim, 0.0), tmp2(dim, 0.0);
    double rate = mean + customers[uid].bu + movies[iid].bi;
    double ru1 = sqrt((double)fd.size()), ru2 = sqrt((double)rated.size());
    for(vector<int>::iterator it = fd.begin(); it != fd.end(); ++it) {
        vector<double>& yj = fdbks[*it].yj;
        for(int i = 0; i < dim; ++i) tmp1[i] += yj[i];
    }
    for(vector<Entry>::iterator it = rated.begin(); it != rated.end(); ++it) {
        vector<double>& yi = movies[it->mid].yi;
        for(int i = 0; i < dim; ++i) tmp2[i] += yi[i];
    }
    for(int i = 0; i < dim; ++i) {
        tmp1[i] /= ru1;
        tmp2[i] /= ru2;
        tmp1[i] += tmp2[i];
        tmp1[i] += pu[i];
        rate += tmp1[i] * qi[i];
    }
    return rate;
}
