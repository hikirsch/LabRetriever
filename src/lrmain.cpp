#include "lrmain.h"

void outputData(const set<string>& lociToRun, const vector<LikelihoodSolver*>& likelihoodSolvers,
        const map<Race, vector<map<string, double> > >& raceToSolverIndexToLocusLogProb,
        const map<Race, vector<double> >& raceToSolverIndexToLogProb,
        const vector<Race> races,
        const string& outputFileName) {
    // TODO: Output in proper format to file:
    stringstream outputStringStream;
    outputStringStream.setf(ios::scientific);
    outputStringStream.precision(4);

    for (unsigned int raceIndex = 0; raceIndex < races.size(); raceIndex++) {
        Race curRace = races[raceIndex];

        const vector<map<string, double> >& solverIndexToLocusLogProb =
                raceToSolverIndexToLocusLogProb.find(curRace)->second;
        const vector<double>& solverIndexToLogProb =
                raceToSolverIndexToLogProb.find(curRace)->second;

        // Output race
        outputStringStream << stringFromRace(curRace) << endl;

        // Output probability header
        outputStringStream << "Probabilities, total";
        for (set<string>::const_iterator iter = lociToRun.begin(); iter != lociToRun.end();
                iter++) {
            const string& locus = *iter;
            outputStringStream << ", " << locus;
        }
        outputStringStream << endl;

        // Output probability data
        for (unsigned int solverIndex = 0; solverIndex < likelihoodSolvers.size(); solverIndex++) {
            map<string, double> locusToLogProb = solverIndexToLocusLogProb[solverIndex];
            outputStringStream << likelihoodSolvers[solverIndex]->name << ", "
                    << exp(solverIndexToLogProb[solverIndex]);
            for (set<string>::const_iterator iter = lociToRun.begin(); iter != lociToRun.end();
                    iter++) {
                const string& locus = *iter;
                double logProb = locusToLogProb[locus];
                outputStringStream << ", " << exp(logProb);
            }
            outputStringStream << endl;
        }

        outputStringStream << endl;

        // Output probability ratio header
        outputStringStream << "Probabilities Ratios, total";
        for (set<string>::const_iterator iter = lociToRun.begin(); iter != lociToRun.end();
                iter++) {
            const string& locus = *iter;
            outputStringStream << ", " << locus;
        }

        outputStringStream << endl;

        // Output probability ratios
        for (unsigned int i = 0; i < likelihoodSolvers.size(); i++) {
            map<string, double> locusToLogProb_i = solverIndexToLocusLogProb[i];
            for (unsigned int j = i + 1; j < likelihoodSolvers.size(); j++) {
                map<string, double> locusToLogProb_j = solverIndexToLocusLogProb[j];
                string ratioName = likelihoodSolvers[i]->name + " to " + likelihoodSolvers[j]->name;
                double diff = solverIndexToLogProb[i] - solverIndexToLogProb[j];
                outputStringStream << ratioName << ", " << exp(diff);
                for (set<string>::const_iterator iter = lociToRun.begin(); iter != lociToRun.end();
                        iter++) {
                    const string& locus = *iter;
                    double logProbDiff = locusToLogProb_i[locus] - locusToLogProb_j[locus];
                    outputStringStream << ", " << exp(logProbDiff);
                }
                outputStringStream << endl;
            }
        }
        outputStringStream << endl;
    }


    string dataToOutput = outputStringStream.str();

    cout << dataToOutput;

    ofstream myFileStream;
    myFileStream.open(outputFileName.c_str());
    myFileStream << dataToOutput;
    myFileStream.close();
}

map<Race, vector<double> > run(const string& executablePath, const string& inputFileName, const string& outputFileName,
        vector<LikelihoodSolver*> likelihoodSolvers) {
    // These are defaults; if specified, will be in the input file.
    double alpha = 0.5;
    double dropinRate = 0.01;
    double dropoutRate = 0.01;
    Race race = ALL;
    IdenticalByDescentProbability identicalByDescentProbability(1, 0, 0);
    map<string, vector<string> > locusToSuspectAlleles;
    map<string, set<string> > locusToAssumedAlleles;
    map<string, vector<set<string> > > locusToUnattributedAlleles;

    vector< vector<string> > inputData = readRawCsv(inputFileName);
    unsigned int csvIndex = 0;
    for (; csvIndex < inputData.size(); csvIndex++) {
        const vector<string>& row = inputData[csvIndex];
        if (row.size() == 0) continue;

        const string& header = row[0];
        if (header == "alpha") {
            if (row.size() <= 1) continue;
            double value = atof(row[1].c_str());
            if (value != 0) {
                alpha = value;
            }
        } else if (header == "Drop-in rate") {
            if (row.size() <= 1) continue;
            double value = atof(row[1].c_str());
            if (value != 0) {
                dropinRate = value;
            }
        } else if (header == "Drop-out rate") {
            if (row.size() <= 1) continue;
            double value = atof(row[1].c_str());
            if (value != 0) {
                dropoutRate = value;
            }
        } else if (header == "Race") {
            if (row.size() <= 1) continue;
            // Currently only expect one race or ALL. Change this to vector if multiple races
            // are needed but not all.
            race = raceFromString(row[1]);
        } else if (header == "IBD Probs") {
            if (row.size() <= 3 || row[1].size() == 0 ||
                    row[2].size() == 0 || row[3].size() == 0 ) continue;
            identicalByDescentProbability.zeroAllelesInCommonProb = atof(row[1].c_str());
            identicalByDescentProbability.oneAlleleInCommonProb = atof(row[2].c_str());
            identicalByDescentProbability.bothAllelesInCommonProb = atof(row[3].c_str());
        } else {
            unsigned int index = header.find("-");
            if (index == string::npos) continue;
            string locus = header.substr(0, index);
            string locusType = header.substr(index+1, header.size());
            set<string> alleleSet;
            vector<string> alleles;
            for (unsigned int i = 1; i < row.size(); i++) {
                string data = row[i];
                if (data.length() != 0) {
                    alleles.push_back(data);
                    alleleSet.insert(data);
                }
            }
            if (locusType == "Assumed") {
                locusToAssumedAlleles[locus] = alleleSet;
            } else if (locusType == "Unattributed") {
                locusToUnattributedAlleles[locus].push_back(alleleSet);
            } else if (locusType == "Suspected") {
                // If there are no suspected alleles, then there's no point of calculating this.
                if (alleles.size() == 0) continue;
                locusToSuspectAlleles[locus] = alleles;
            }
        }
    }

    vector<Race> races;
    // Determine the races to run.
    if (race == ALL) {
        for (int r = (int) RACE_START; r < (int) RACE_END; r += 1) {
            races.push_back((Race) r);
        }
    } else {
        races.push_back(race);
    }

    map<Race, vector<double> > raceToSolverIndexToLogProb;
    map<Race, vector<map<string, double> > > raceToSolverIndexToLocusLogProb;

    for (unsigned int raceIndex = 0; raceIndex < races.size(); raceIndex++) {
        Race r = races[raceIndex];
        raceToSolverIndexToLogProb.insert(
                pair<Race, vector<double> >(r, vector<double>(likelihoodSolvers.size(), 0)));
        raceToSolverIndexToLocusLogProb.insert(
                pair<Race, vector<map<string, double> > >(r,
                        vector<map<string, double> >(likelihoodSolvers.size())));
    }


    // Check for loci where there is a suspect allele, and the assumed and unattributed
	// rows are present (but possibly empty).
    set<string> lociToRun;
    for (map<string, vector<string> >::const_iterator iter = locusToSuspectAlleles.begin();
            iter != locusToSuspectAlleles.end(); iter++) {
        const string& locus = iter->first;
        if (locusToAssumedAlleles.find(locus) != locusToAssumedAlleles.end() &&
                locusToUnattributedAlleles.find(locus) != locusToUnattributedAlleles.end()) {
            lociToRun.insert(locus);
        }
    }

    // Create configurations and run on likelihood solvers.
    for (set<string>::const_iterator iter = lociToRun.begin();
            iter != lociToRun.end(); iter++) {
        string locus = *iter;

        vector<set<string> > unattributedAlleles = locusToUnattributedAlleles[locus];
        set<string> assumedAlleles = locusToAssumedAlleles[locus];
        vector<string> suspectAlleles = locusToSuspectAlleles[locus];

        set<string> allAlleles;
        allAlleles.insert(assumedAlleles.begin(), assumedAlleles.end());
        allAlleles.insert(suspectAlleles.begin(), suspectAlleles.end());
        for (unsigned int i = 0; i < unattributedAlleles.size(); i++) {
            allAlleles.insert(unattributedAlleles[i].begin(), unattributedAlleles[i].end());
        }

        AlleleProfile suspectProfile;
        vector<ReplicateData> replicateDatas;

        // If only one suspect allele is present, then assume it means that there are two of them.
        // TODO: fix quick hack for this:
        if (suspectAlleles.size() == 1) {
            suspectProfile.addAllele(suspectAlleles[0]);
        }

        for (unsigned int i = 0; i < suspectAlleles.size(); i++) {
            suspectProfile.addAllele(suspectAlleles[i]);
        }
        for (unsigned int unattribIndex = 0; unattribIndex < unattributedAlleles.size();
                unattribIndex++) {
            replicateDatas.push_back(ReplicateData::fromUnattributedAndMaskedAlleles(
                    unattributedAlleles[unattribIndex], assumedAlleles));
        }

        map<Race, map<string, unsigned int> > raceToAlleleCounts =
                getAlleleCountsFromFile(executablePath + "Allele Frequency Tables/" + locus + "_B.count.csv",
                        races);

        for (unsigned int raceIndex = 0; raceIndex < races.size(); raceIndex++) {
            Race curRace = races[raceIndex];
            map<string, unsigned int> alleleCounts = raceToAlleleCounts[curRace];

            // Edit the allele counts so that every allele is given at least 5 counts, even if the
            // allele does not appear in the table.
            for (set<string>::const_iterator iter = allAlleles.begin(); iter != allAlleles.end();
                    iter++) {
                const string& allele = *iter;
                if (alleleCounts.count(allele) == 0 || alleleCounts[allele] < 5) {
                    alleleCounts[allele] = 5;
                }
            }

            map<string, double> alleleProp =
                    getAlleleProportionsFromCounts(alleleCounts, suspectProfile);

            Configuration config(suspectProfile, replicateDatas, alleleProp,
                    identicalByDescentProbability, dropoutRate, dropinRate, alpha);

            for (unsigned int solverIndex = 0; solverIndex < likelihoodSolvers.size();
                    solverIndex++) {
                LikelihoodSolver* solver = likelihoodSolvers[solverIndex];
                double logLikelihood = solver->getLogLikelihood(config);
                raceToSolverIndexToLocusLogProb[curRace][solverIndex][locus] = logLikelihood;
                raceToSolverIndexToLogProb[curRace][solverIndex] += logLikelihood;
            }
        }
    }

    // TODO: Output in proper format to file:
    outputData(lociToRun, likelihoodSolvers, raceToSolverIndexToLocusLogProb,
            raceToSolverIndexToLogProb, races, outputFileName);
    return raceToSolverIndexToLogProb;
}

int main(int argc, char *argv[]) {
    vector<LikelihoodSolver*> solversToUse;

    if (argc < 2) {
        std::cout << "Usage is <inputfile> <outputfile> [<[01][0123]>, ...]\n";
        std::cout << "first digit is number of suspects, second is number of unknowns.\n";
        std::cout << "\nexample: for no suspect, one unknown and one suspect, one unknown\n";
        std::cout << argv[0] << " input.csv output.csv 01 11\n\n";
        return -1;
    }
    if (argc > 2) {
        for (int i = 3; i < argc; i++) {
            if (argv[i][0] == '0') {
                switch(argv[i][1]) {
                case '4':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::NO_SUSPECT_FOUR_UNKNOWNS));
                    break;
                case '3':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::NO_SUSPECT_THREE_UNKNOWNS));
                    break;
                case '2':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::NO_SUSPECT_TWO_UNKNOWNS));
                    break;
                default:
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::NO_SUSPECT_ONE_UNKNOWN));
                }
            } else { // one suspect
                switch(argv[i][1]) {
                case '3':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::ONE_SUSPECT_THREE_UNKNOWNS));
                    break;
                case '2':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::ONE_SUSPECT_TWO_UNKNOWNS));
                    break;
                case '1':
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::ONE_SUSPECT_ONE_UNKNOWN));
                    break;
                default:
                    solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::ONE_SUSPECT_NO_UNKNOWNS));
                }
            }
        }
    } else {
        solversToUse.push_back(LikelihoodSolver::getSolver(LikelihoodSolver::NO_SUSPECT_ONE_UNKNOWN));
    }

    string executablePath = string(argv[0]);

    // The executable file name is 'labr' but on windows is 'labr.exe'
    int fileNameLength = 4;
    if (executablePath.find(".exe") != string::npos) {
        fileNameLength += 4;
    }

    run(executablePath.substr(0, executablePath.length() - fileNameLength), argv[1], argv[2], solversToUse);
}
