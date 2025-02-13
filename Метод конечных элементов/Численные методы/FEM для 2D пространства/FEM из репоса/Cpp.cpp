#include "Header.h"
#include "pch.h"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <fstream>
#include <iostream>

struct Element
{
	void CalculateStiffnessMatrix(const Eigen::Matrix3f& D, std::vector<Eigen::Triplet<float> >& triplets);

	Eigen::Matrix<float, 3, 6> B;
	int nodesIds[3];
};
struct Constraint
{
	enum Type
	{
		UX = 1 << 0,
		UY = 1 << 1,
		UXY = UX | UY
	};
	int node;
	Type type;
};
int                         nodesCount;
Eigen::VectorXf             nodesX;
Eigen::VectorXf             nodesY;
Eigen::VectorXf             loads;
std::vector< Element >      elements;
std::vector< Constraint >   constraints;
float Thickness = 1.0f;
void Element::CalculateStiffnessMatrix(const Eigen::Matrix3f& D, std::vector<Eigen::Triplet<float> >& triplets)
{
	Eigen::Vector3f x, y;
	x << nodesX[nodesIds[0]], nodesX[nodesIds[1]], nodesX[nodesIds[2]];
	y << nodesY[nodesIds[0]], nodesY[nodesIds[1]], nodesY[nodesIds[2]];

	Eigen::Matrix3f C;
	C << Eigen::Vector3f(1.0f, 1.0f, 1.0f), x, y;

	Eigen::Matrix3f IC = C.inverse();

	for (int i = 0; i < 3; i++)
	{
		B(0, 2 * i + 0) = IC(1, i);
		B(0, 2 * i + 1) = 0.0f;
		B(1, 2 * i + 0) = 0.0f;
		B(1, 2 * i + 1) = IC(2, i);
		B(2, 2 * i + 0) = IC(2, i);
		B(2, 2 * i + 1) = IC(1, i);
	}
	Eigen::Matrix<float, 6, 6> K = Thickness * B.transpose() * D * B * C.determinant() / 2.0f;

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			Eigen::Triplet<float> trplt11(2 * nodesIds[i] + 0, 2 * nodesIds[j] + 0, K(2 * i + 0, 2 * j + 0));
			Eigen::Triplet<float> trplt12(2 * nodesIds[i] + 0, 2 * nodesIds[j] + 1, K(2 * i + 0, 2 * j + 1));
			Eigen::Triplet<float> trplt21(2 * nodesIds[i] + 1, 2 * nodesIds[j] + 0, K(2 * i + 1, 2 * j + 0));
			Eigen::Triplet<float> trplt22(2 * nodesIds[i] + 1, 2 * nodesIds[j] + 1, K(2 * i + 1, 2 * j + 1));

			triplets.push_back(trplt11);
			triplets.push_back(trplt12);
			triplets.push_back(trplt21);
			triplets.push_back(trplt22);
		}
	}
}

void SetConstraints(Eigen::SparseMatrix<float>::InnerIterator& it, int index)
{
	if (it.row() == index || it.col() == index)
	{
		it.valueRef() = it.row() == it.col() ? 1.0f : 0.0f;
	}
}

void ApplyConstraints(Eigen::SparseMatrix<float>& K, const std::vector<Constraint>& constraints)
{
	std::vector<int> indicesToConstraint;

	for (std::vector<Constraint>::const_iterator it = constraints.begin(); it != constraints.end(); ++it)
	{
		if (it->type & Constraint::UX)
		{
			indicesToConstraint.push_back(2 * it->node + 0);
		}
		if (it->type & Constraint::UY)
		{
			indicesToConstraint.push_back(2 * it->node + 1);
		}
	}

	for (int k = 0; k < K.outerSize(); ++k)
	{
		for (Eigen::SparseMatrix<float>::InnerIterator it(K, k); it; ++it)
		{
			for (std::vector<int>::iterator idit = indicesToConstraint.begin(); idit != indicesToConstraint.end(); ++idit)
			{
				SetConstraints(it, *idit);
			}
		}
	}
}

extern "C" __declspec(dllexport) void Creat() {
	char path1[] = "MKV.inp";
	char path2[] = "MKV.PIL";

	std::ifstream infile(path1);
	std::ofstream outfile(path2);

	float poissonRatio, youngModulus; // �������� ���������, ��������� // 0.3 2000
	infile >> poissonRatio >> youngModulus >> Thickness;

	Eigen::Matrix3f D;
	D <<
		1.0f, poissonRatio, 0.0f,
		poissonRatio, 1.0, 0.0f,
		0.0f, 0.0f, (1.0f - poissonRatio) / 2.0f;

	D *= youngModulus / (1.0f - pow(poissonRatio, 2.0f));

	infile >> nodesCount; // ������ ������ � ������������ ����� // 4
	nodesX.resize(nodesCount);
	nodesY.resize(nodesCount);

	for (int i = 0; i < nodesCount; ++i) // ���������� ����� � ����� // 0.0 0.0 // 1.0 0.0 // 0.0 1.0 // 1.0 1.0
	{
		infile >> nodesX[i] >> nodesY[i];
	}

	int elementCount; // ���������� ��������� // 2
	infile >> elementCount;

	for (int i = 0; i < elementCount; ++i) // ������� ����� ��� ������� �������� // 0 1 2 // 1 3 2
	{
		Element element;
		infile >> element.nodesIds[0] >> element.nodesIds[1] >> element.nodesIds[2];
		elements.push_back(element);
	}

	int constraintCount; // ����� ����������� // 2
	infile >> constraintCount;

	for (int i = 0; i < constraintCount; ++i) // ������ ����������� // 0 3 // 1 2
	{
		Constraint constraint;
		int type;
		infile >> constraint.node >> type;
		constraint.type = static_cast<Constraint::Type>(type);
		constraints.push_back(constraint);
	}

	int loadsCount; // ������� ����������� ������ // 2

	loads.resize(2 * nodesCount);
	loads.setZero();

	infile >> loadsCount;


	for (int i = 0; i < loadsCount; ++i) // ���������� ������� ��� // 2 0.0 1.0 // 3 0.0 1.0
	{
		int node;
		float x, y;
		infile >> node >> x >> y;
		loads[2 * node + 0] = x;
		loads[2 * node + 1] = y;
	}





	std::vector<Eigen::Triplet<float> > triplets;
	for (std::vector<Element>::iterator it = elements.begin(); it != elements.end(); ++it)
	{
		it->CalculateStiffnessMatrix(D, triplets);
	}

	Eigen::SparseMatrix<float> globalK(2 * nodesCount, 2 * nodesCount);
	globalK.setFromTriplets(triplets.begin(), triplets.end());

	ApplyConstraints(globalK, constraints);

	//Eigen::SimplicialLDLT<Eigen::SparseMatrix<float> > solver(globalK);

	//Eigen::VectorXf displacements = solver.solve(loads);

	outfile << "Global stiffness matrix:\n";
	//std::cout << static_cast >& > (globalK) << std::endl;

	outfile << "Loads vector:" << std::endl << loads << std::endl << std::endl;

	Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>> solver(globalK);

	Eigen::VectorXf displacements = solver.solve(loads);

	outfile << "Displacements vector:" << std::endl << displacements << std::endl;

	outfile << "Stresses:" << std::endl;

	for (std::vector<Element>::iterator it = elements.begin(); it != elements.end(); ++it)
	{
		Eigen::Matrix<float, 6, 1> delta;
		delta << displacements.segment<2>(2 * it->nodesIds[0]),
			displacements.segment<2>(2 * it->nodesIds[1]),
			displacements.segment<2>(2 * it->nodesIds[2]);

		Eigen::Vector3f sigma = D * it->B * delta;
		float sigma_mises = sqrt(sigma[0] * sigma[0] - sigma[0] * sigma[1] + sigma[1] * sigma[1] + 3.0f * sigma[2] * sigma[2]);

		outfile << sigma_mises << std::endl;
		//outfile << sigma_mises << std::endl;
	}


	//outfile << displacements << std::endl;
	/*
	for (std::vector<Element>::iterator it = elements.begin(); it != elements.end(); ++it)
	{
		Eigen::Matrix<float, 6, 1> delta;
		delta << displacements.segment<2>(2 * it->nodesIds[0]),
			displacements.segment<2>(2 * it->nodesIds[1]),
			displacements.segment<2>(2 * it->nodesIds[2]);

		Eigen::Vector3f sigma = D * it->B * delta;
		float sigma_mises = sqrt(sigma[0] * sigma[0] - sigma[0] * sigma[1] + sigma[1] * sigma[1] + 3.0f * sigma[2] * sigma[2]);

		outfile << sigma_mises << std::endl;
	}
	*/
}