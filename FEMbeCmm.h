//#pragma once
//=============================================================================
// This plugin example illustrates how to create a new material. 
// It requires FEBio 2.5 (or up)
//
// Author Steve Maas
// Copyright (c) 2015 - 2016
// All rights reserved
//
//=============================================================================

//-----------------------------------------------------------------------------
// We need to include this file since our new material class will inherit from
// FEElasticMaterial which is defined in this include files.
#include "FEBioMech/FEElasticMaterial.h"
#include <iostream>								// to use cin.get()

class FEBIOMECH_API GRMaterialPoint : public FEMaterialPointData
{
public:
	GRMaterialPoint(FEMaterialPointData *pt) : FEMaterialPointData(pt) {};

	FEMaterialPointData* Copy() override;

	void Init() override;
	void Serialize(DumpStream& ar) override;

public:
	// original (o) homeostatic data
	double		m_Jo;		//!< Jacobian at o
	double		m_svo;		//!< volumetric stress at o
	mat3ds		m_smo;		//!< Cauchy stress tensor for smooth muscle cells at o
	mat3ds		m_sco;		//!< Cauchy stress tensor for all collagen fiber families at o
	mat3d		m_Fio;		//!< inverse of deformation gradient tensor at o
	double		m_Jh;		//!< Jacobian at h
	mat3d		m_Fih;		//!< inverse of deformation gradient tensor at h

	// evolved homeostatic (h) data
	double		m_phic;		//!< total mass fraction of all collagen fiber families at h
	double		m_Iemax;	//!< maximum value of Ie achieved over the loading history up until the current G&R time
};

//-----------------------------------------------------------------------------
// This material class implements a neo-Hookean constitutive model. 
// Since it is a (compressible, coupled) hyper-elatic material, it needs to inherit
// from FEElasticMaterial. 
class FEMbeCmm : public FEElasticMaterial
{
public:
	FEMbeCmm(FEModel* pfem);

	//! create material point data for this material
	// FEMaterialPointData* CreateMaterialPointData() override;

public:
	// The constructor is called when an instance of this class is created.
	// All classes registered by the framework must take the FEModel* as the only
	// parameter in the constructor, even if the class does not need it (which most often
	// will be the case). For material classes, the FEModel parameter is passed to the 
	// base class in the initialization list.

	// setting m_secant_tangent = true so FESolidMaterial uses SecantTangent
	// (allows minor symmetry only tangents) instead of Tangent (minor and major symmetries)
	// 	 { return m_secant_tangent; }
    bool m_secant_tangent;   //!< flag for using secant tangent

    DECLARE_FECORE_CLASS();

public:
	// function to perform material evaluation. calculates stress and tangent to avoid code duplication
	void StressTangent(FEMaterialPoint& mp, mat3ds& stress, tens4dmm& tangent);

	// This function calculates the spatial (i.e. Cauchy or true) stress.
	// It takes one parameter, the FEMaterialPoint and returns a mat3ds object
	// which is a symmetric second-order tensor.
	virtual mat3ds Stress(FEMaterialPoint& pt) override {
		mat3ds stress;
		tens4dmm tangent;
		StressTangent(pt, stress, tangent);
		return stress;
	}

	// This function calculates the spatial elasticity tangent tensor. 
	// It takes one parameter, the FEMaterialPoint and retursn a tens4ds object
	// which is a fourth-order tensor with major and minor symmetries.
	virtual tens4ds Tangent(FEMaterialPoint& pt) override {
		tens4ds tangent;
		return tangent;
	};

	// minor symmetries only
	virtual tens4dmm SecantTangent(FEMaterialPoint& pt) {
		mat3ds stress;
		tens4dmm tangent;
		StressTangent(pt, stress, tangent);
		return tangent;
	}

};
