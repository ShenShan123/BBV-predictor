#include <random>
#include <fstream>
#include <iostream>

void randMatrix(int e, int b, std::ofstream & file)
{
    // construct a trivial random generator engine from a time-based seed:
    std::random_device seed;
    static std::mt19937 engine(seed());
    static std::uniform_real_distribution<double> unif(-1, 1);
    
    file << "double randM[" << e << "][" << b << "] = {\n";
    for (int i = 0; i < e - 1; ++i) {
    	file << "\t{";
    	/* elements in each row */
    	for (int j = 0; j < b - 1; ++j)
    		file << unif(engine) << ", ";
    	/* last element */
    	file << unif(engine) << "},\n";
    }

    /* last row */
    file << "\t{";
    /* elements in each row */
    for (int j = 0; j < b - 1; ++j)
    	file << unif(engine) << ", ";
    /* last element */
    file << unif(engine) << "}\n";
    file << "};" << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc != 3){
		std::cerr << "invalid arguments!! Require the size of random matrix!!\n";
		exit(-1);
	}

	std::ofstream fout("random_matrix.h", std::ios::out);
	fout << "#pragma once\n\n";

	std::string nstr(argv[1]);
	std::string mstr(argv[2]);
	std::cout << "Matrix num row: " << nstr << " num column: " << mstr << std::endl;

	randMatrix(stoi(nstr), stoi(mstr), fout);

	fout.close();
}