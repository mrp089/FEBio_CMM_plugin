//#include "stdafx.h"
#include "FEMbeCmm.h"
#include "FEBioMech/FEElasticMaterial.h"
#include "FECore/FEAnalysis.h"					// to get end time
#include "FECore/FEModel.h"						// to get current time
#include "FECore/log.h"							// to print to log file and/or screen
#include <iostream>								// to use cin.get()
#include <sstream>
#include <signal.h>
#define _USE_MATH_DEFINES						// to introduce pi constant (1/2)
#include <math.h>								// to introduce pi constant (2/2)

// define the material parameters
BEGIN_FECORE_CLASS(FEMbeCmm, FEElasticMaterial)
    ADD_PARAMETER(m_secant_tangent, "secant_tangent");
END_FECORE_CLASS();

FEMbeCmm::FEMbeCmm(FEModel* pfem) : FEElasticMaterial(pfem)
{
    m_secant_tangent = true;
}

FEMaterialPointData* GRMaterialPoint::Copy()
{
	GRMaterialPoint* pt = new GRMaterialPoint(*this);
    if (m_pNext) pt->m_pNext = m_pNext->Copy();
    return pt;
}

void GRMaterialPoint::Init()
{
	FEMaterialPointData::Init();

	m_Jo = 1;
	m_svo = 0;
	m_smo.zero();
	m_sco.zero();
	m_Fio.unit();
	m_Jh = 1;
	m_Fih.unit();

	m_phic = 0;
	m_Iemax = 0;
}

void GRMaterialPoint::Serialize(DumpStream& ar)
{
	FEMaterialPointData::Serialize(ar);
	ar & m_Jo & m_svo & m_smo & m_sco & m_Fio & m_Jh & m_Fih & m_phic & m_Iemax;
}

FEMaterialPointData* FEMbeCmm::CreateMaterialPointData() 
{ 
	return new GRMaterialPoint(new FEElasticMaterialPoint); 
}

void FEMbeCmm::StressTangent(FEMaterialPoint& mp, mat3ds& stress, tens4dmm& tangent)
{		
	// The FEMaterialPoint classes are stored in a linked list. The specific material
	// point data needed by this function can be accessed using the ExtractData member.
	// In this case, we want to FEElasticMaterialPoint data since it stores the deformation
	// information that is needed to evaluate the stress.
	FEElasticMaterialPoint& et = *mp.ExtractData<FEElasticMaterialPoint>();
	GRMaterialPoint& pt = *mp.ExtractData<GRMaterialPoint>();

	// We'll need the deformation gradient and its determinant in this function.
	// Note that we don't take the determinant of F directly (using mat3d::det)
	// but instead use the m_J member variable of FEMaterialPoint.
	const mat3d &F = et.m_F;
	const double J = et.m_J;

	const double eps = std::numeric_limits<double>::epsilon();

	// get current and end times
	const double t = GetFEModel()->GetTime().currentTime;

	const double endtime = 11.0;							// 11.0 | 31.0-32.0 (TEVG)
	const double partialtime = endtime;			// partialtime <= endtime | 10.0 | 10.4 (for TI calculation)
	const double sgr = min(t,partialtime);		// min(t,partialtime) | min(t,9.0)

	// retrieve material position
	const vec3d  X = mp.m_r0;

	const double imper = 0.00;					// imper > 0 for TORTUOSITY (see Matlab script <NodesElementsAsy.m>) | 0.00 | 20.0
	const double rIo = 0.6468;					// 0.6468 | 0.5678
	const double hwaves = 2.0;
	const double lo = 30.0;
	const vec3d  Xcl = {0.0, imper/100.0*rIo*sin(hwaves*M_PI*X.z/lo), X.z};		// center line

	vec3d NX = {X.x-Xcl.x,X.y-Xcl.y,X.z-Xcl.z};								// radial vector

	const double ro = sqrt(NX*NX);

	NX /= ro;

	// retrieve local element basis directions
	vec3d N[3];

	// pointwise, consistent with mesh generated with Matlab script <NodesElementsAsy.m>
	N[2] = {0.0, imper/100.0*rIo*hwaves*M_PI/lo*cos(hwaves*M_PI*X.z/lo), 1.0}; N[2] = N[2]/sqrt(N[2]*N[2]);		// axial = d(Xcl)/d(z)
	N[1] = {-NX.y, NX.x, NX.z};																					// circumferential
	N[0] = N[2]^N[1];

	// elementwise, from input file
	// N[2] = pt.m_Q.col(0); N[1] = pt.m_Q.col(1); N[0] = pt.m_Q.col(2);							// axial, circumferential, radial

	const double phieo = 0.34;								// 0.34 (CMAME | KNOCKOUTS) | 1.00 (TEVG) | 1.0/3.0 (TEVG)
	const double phimo = 0.5*(1.0-phieo);
	const double phico = 0.5*(1.0-phieo);

	const double eta = 1.0;									// 1.0 | 1.0/3.0 (for uniform cases) | 0.714

	const double mu = 89.71;
	const double Get = 1.90;
	const double Gez = 1.62;

	double alpha = 0.522;								// original orientation of diagonal collagen | 0.522 (CMAME | KNOCKOUTS) | 0.8713 (TEVG)

	// original homeostatic parameters (adaptive)

	// passive
	const double cm = 261.4;									// 261.4 (CMAME | KNOCKOUTS) | 46.61 (TEVG)
	const double dm = 0.24;
	const double Gm = 1.20;
	const double cc = 234.9;									// 234.9 (CMAME | KNOCKOUTS) | 328.475 (TEVG)
	const double dc = 4.08;
	const double Gc = 1.25;

	// orientation fractions for collagen
	const double betat = 0.056;
	const double betaz = 0.067;
	const double betad = 0.5*(1.0 - betat - betaz);

	vec3d  Np = N[1]*sin(alpha)+N[2]*cos(alpha);		// original diagonal fiber direction
	vec3d  Nn = N[1]*sin(alpha)-N[2]*cos(alpha);		// idem for symmetric

	// active
	const double Tmax = 250.0 * 0.0;							// 250.0 | 50.0 | 150.0 (for uniform cases, except for contractility -> 250)
	const double lamM = 1.1;
	const double lam0 = 0.4;
	const double CB = sqrt(log(2.0));							// such that (1-exp(-CB^2)) = 0.5
	const double CS = 0.5*CB * 1.0;							// such that (1-exp( -C^2)) = 0.0 for lt = 1/(1+CB/CS)^(1/3) = 0.7 and (1-exp(-C^2)) = 0.75 for lt = 2.0

	const double KsKi = 0.35;
	const double EPS  = 1.0+(1.0-1.0)*(sgr-1.0)/(endtime-1.0);

	const double KfKi   = 1.0;
	const double inflam = 0.0*(sgr-1.0)/(endtime-1.0);

	const double aexp = 1.0;									// 1.0 (KNOCKOUTS | TEVG) | 0.0 (CMAME | TORTUOSITY)

	const double delta = 0.0;

	// compute U from polar decomposition of deformation gradient tensor
	mat3ds U; mat3d R; F.right_polar(R,U);

	double eigenval[3]; vec3d eigenvec[3];
	U.eigen2(eigenval,eigenvec);

	// right Cauchy-Green tensor and its inverse
	const mat3ds C = et.RightCauchyGreen();
	const mat3ds Ci = C.inverse();

	// Ge from spectral decomposition
	const mat3ds Ge = 1.0/Get/Gez*dyad(N[0]) + Get*dyad(N[1]) + Gez*dyad(N[2]);
	
	// stress for elastin
	const mat3ds Se = (phieo*mu*Ge*Ge).sym();						// phieo*Ge*Sehat*Ge = phieo*Ge*(mu*I)*Ge

	// computation of the second Piola-Kirchhoff stress
	mat3ds S;

	// define identity tensor and some useful dyadic products of the identity tensor
	const mat3dd  I(1.0);
	const tens4ds IxI = dyad1s(I);
	const tens4ds IoI = dyad4s(I);
	const tens4dmm IxIss = tens4dmm(IxI);							// IxI in tens4dmm form
	const tens4dmm IoIss = tens4dmm(IoI);							// IoI in tens4dmm form

	// spatial moduli for elastin
	tens4ds ce(0.0);								// phieo/J*(FcF:GecGe:Cehat:GecGe:FTcFT) = phieo/J*(FcF:GecGe:0:GecGe:FTcFT)

	// computation of spatial moduli
	tens4dmm css;
	mat3ds sfpro;
	if (t <= 1.0 + eps) {
		// compute stress
		const double Jdep = 0.9999;
		const double lm = 1.0e3*mu;
		
		const double lt = (F*N[1]).norm();
		const double lz = (F*N[2]).norm();
		const double lp = (F*Np).norm();
		const double ln = (F*Nn).norm();
		
		const double lmt2 = (Gm*lt)*(Gm*lt);
		const double lct2 = (Gc*lt)*(Gc*lt);
		const double lcz2 = (Gc*lz)*(Gc*lz);
		const double lcp2 = (Gc*lp)*(Gc*lp);
		const double lcn2 = (Gc*ln)*(Gc*ln);
		
		// passive
		const mat3ds Sm = (cm*(lmt2-1.0)*exp(dm*(lmt2-1.0)*(lmt2-1.0))*(Gm*Gm)*dyad(N[1]));
		const mat3ds Sc =	(cc*(lct2-1.0)*exp(dc*(lct2-1.0)*(lct2-1.0))*(Gc*Gc)*dyad(N[1])*betat +
							 cc*(lcz2-1.0)*exp(dc*(lcz2-1.0)*(lcz2-1.0))*(Gc*Gc)*dyad(N[2])*betaz +
					 	 	 cc*(lcp2-1.0)*exp(dc*(lcp2-1.0)*(lcp2-1.0))*(Gc*Gc)*dyad( Np )*betad +
							 cc*(lcn2-1.0)*exp(dc*(lcn2-1.0)*(lcn2-1.0))*(Gc*Gc)*dyad( Nn )*betad );

		// active
		const mat3ds Sa = Tmax*(1.0-exp(-CB*CB))*(1.0-pow((lamM-1.0)/(lamM-lam0),2))*(lt*lt)*dyad(N[1]);
		
		const mat3ds Sx = Se + phimo * Sm + phico * Sc + phimo * Sa;
		
		S = Sx + Ci*lm*log(Jdep*J);
		
		mat3d u(U);
		
		pt.m_Jo    = J;
		pt.m_svo   = 1.0/3.0/J*S.dotdot(C);
		pt.m_smo   = 1.0/J*(u*(Sm*u)).sym();
		pt.m_sco   = 1.0/J*(u*(Sc*u)).sym();
		pt.m_Fio   = F.inverse();
		pt.m_Jh    = pt.m_Jo;
		pt.m_Fih   = pt.m_Fio;
		pt.m_phic  = phico;

		// compute tangent
		const mat3ds tent = dyad(F*N[1]);
		const mat3ds tenz = dyad(F*N[2]);
		const mat3ds tenp = dyad(F*Np);
		const mat3ds tenn = dyad(F*Nn);

		// passive
		tens4ds cf = phimo*(2.0*cm*(1.0+2.0*dm*(lmt2-1.0)*(lmt2-1.0))*exp(dm*(lmt2-1.0)*(lmt2-1.0))*pow(Gm,4)*dyad1s(tent))      +
					 phico*(2.0*cc*(1.0+2.0*dc*(lct2-1.0)*(lct2-1.0))*exp(dc*(lct2-1.0)*(lct2-1.0))*pow(Gc,4)*dyad1s(tent)*betat +
							2.0*cc*(1.0+2.0*dc*(lcz2-1.0)*(lcz2-1.0))*exp(dc*(lcz2-1.0)*(lcz2-1.0))*pow(Gc,4)*dyad1s(tenz)*betaz +
							2.0*cc*(1.0+2.0*dc*(lcp2-1.0)*(lcp2-1.0))*exp(dc*(lcp2-1.0)*(lcp2-1.0))*pow(Gc,4)*dyad1s(tenp)*betad +
							2.0*cc*(1.0+2.0*dc*(lcn2-1.0)*(lcn2-1.0))*exp(dc*(lcn2-1.0)*(lcn2-1.0))*pow(Gc,4)*dyad1s(tenn)*betad);

		// active
		tens4ds ca = phimo*2.0*Tmax*(1.0-exp(-CB*CB))*(1.0-pow((lamM-1.0)/(lamM-lam0),2))*dyad1s(tent);

		cf /= J; ca /= J;

		tens4ds c = ce + cf + ca;

		c += lm/J*(IxI-2.0*log(Jdep*J)*IoI);

		css = tens4dmm(c);		// c in tens4dmm form
	}
	else if (t <= partialtime + eps) {
		// compute stress
		const double    Jo = pt.m_Jo;
		const double   svo = pt.m_svo;
		mat3ds        &smo = pt.m_smo;
		mat3ds        &sco = pt.m_sco;
		const mat3d    Fio = pt.m_Fio;
		double       &phic = pt.m_phic;
		
		phic = phico;																// initial guess
		double dRdc = J/Jo*(1.0+phimo/phico*eta*pow(J/Jo*phic/phico,eta-1.0));		// initial tangent d(R)/d(phic)
		double Rphi = phieo+phimo*pow(J/Jo*phic/phico,eta)+J/Jo*phic-J/Jo;			// initial residue
		do {																		// local iterations to obtain phic
			phic = phic-Rphi/dRdc;													// phic
			dRdc = J/Jo*(1.0+phimo/phico*eta*pow(J/Jo*phic/phico,eta-1.0));			// tangent
			Rphi = phieo+phimo*pow(J/Jo*phic/phico,eta)+J/Jo*phic-J/Jo;				// update residue
		} while (abs(Rphi) > sqrt(eps));											// && abs(Rphi/Rphi0) > sqrt(eps) && j<10
		phic = phic-Rphi/dRdc;														// converge phase -> phic (updated in material point memory)

		const double phim = phimo/(J/Jo)*pow(J/Jo*phic/phico,eta);	// phim from <J*phim/phimo=(J*phic/phico)^eta>
		
		// recompute remodeled original stresses for smc and collagen (from remodeled natural configurations)

		const double lto = (Fio.inverse()*N[1]).norm();
		const double lzo = (Fio.inverse()*N[2]).norm();
		const double lpo = (Fio.inverse()*Np).norm();					// original referential stretch for deposition stretch calculation
		const double lno = (Fio.inverse()*Nn).norm();					// idem for symmetric
		
		const double lmt2 = (Gm*lto)*(Gm*lto);
		const double lct2 = (Gc*lto)*(Gc*lto);
		const double lcz2 = (Gc*lzo)*(Gc*lzo);
		const double lcp2 = (Gc*lpo)*(Gc*lpo);						// deposition stretch calculation (computational purposes)
		const double lcn2 = (Gc*lno)*(Gc*lno);						// idem for symmetric

		const double lr = (F*(Fio*N[0])).norm();						// lr -> 1 for F -> Fo
		const double lt = (F*(Fio*N[1])).norm();						// lt -> 1 for F -> Fo
		const double lz = (F*(Fio*N[2])).norm();						// lz -> 1 for F -> Fo

		alpha = atan(tan(alpha)*pow(lt/lz,aexp));				// update alpha
		Np = N[1]*sin(alpha)+N[2]*cos(alpha);					// update diagonal fiber vector
		Nn = N[1]*sin(alpha)-N[2]*cos(alpha);					// idem for symmetric
		
		// passive
		const mat3ds Smo = (cm*(lmt2-1.0)*exp(dm*(lmt2-1.0)*(lmt2-1.0))*(Gm*Gm)*dyad(N[1]));
		const mat3ds Sco = (cc*(lct2-1.0)*exp(dc*(lct2-1.0)*(lct2-1.0))*(Gc*Gc)*dyad(N[1])*betat +
					  cc*(lcz2-1.0)*exp(dc*(lcz2-1.0)*(lcz2-1.0))*(Gc*Gc)*dyad(N[2])*betaz +
					  cc*(lcp2-1.0)*exp(dc*(lcp2-1.0)*(lcp2-1.0))*(Gc*Gc)*dyad( Np )*betad +
					  cc*(lcn2-1.0)*exp(dc*(lcn2-1.0)*(lcn2-1.0))*(Gc*Gc)*dyad( Nn )*betad );

		// active
		const mat3ds Sao = Tmax*(1.0-exp(-CB*CB))*(1.0-pow((lamM-1.0)/(lamM-lam0),2))*(lto*lto)*dyad(N[1]);
		
		mat3ds Uo; mat3d Ro; (Fio.inverse()).right_polar(Ro,Uo);	// Uo from polar decomposition
		mat3d  uo(Uo);
		
		smo = 1.0/Jo*(uo*(Smo*uo)).sym();
		sco = 1.0/Jo*(uo*(Sco*uo)).sym();

		const mat3ds sao = 1.0/Jo*(uo*(Sao*uo)).sym();

		// compute current stresses
		
		double rIrIo = ro/rIo*lt-(ro-rIo)/rIo*lr;				// rIrIo -> rIorIo = 1 for F -> Fo

		mat3ds sNm = phim*smo;									// phim*smhato = phim*smo
		mat3ds sNc = phic*sco;									// phic*schato = phic*sco

		const mat3ds sNf = sNm + sNc;

		const double Cratio = CB-CS*(EPS*pow(rIrIo,-3)-1.0);
		mat3ds sNa; sNa.zero();
		if (Cratio>0) sNa = phim*(1.0-exp(-Cratio*Cratio))/(1.0-exp(-CB*CB))*sao;

		const mat3ds Ui = U.inverse();            					// inverse of U
		const mat3d  ui(Ui);

		const mat3ds Sf = J*(ui*sNf*ui).sym();						// J*Ui*sNf*Ui
		const mat3ds Sa = J*(ui*sNa*ui).sym();						// J*Ui*sNa*Ui
		
		const mat3ds Sx = Se + Sf + Sa;

		const double p = 1.0/3.0/J*Sx.dotdot(C) - svo/(1.0-delta)*(1.0+KsKi*(EPS*pow(rIrIo,-3)-1.0)-KfKi*inflam);		// Ups = 1 -> p
		
		S = Sx - J*p*Ci;
		
		pt.m_Jh  = J;
		pt.m_Fih = F.inverse();

		//compute tangent

		// compute current stresses
		sNm = smo;										// phim*smhato = phim*smo
		sNc = sco;										// phic*schato = phic*sco

		// 2nd P-K stresses
		const mat3ds Sm = J*(ui*sNm*ui).sym();						// J*Ui*sNm*Ui
		const mat3ds Sc = J*(ui*sNc*ui).sym();						// J*Ui*sNc*Ui

		// associated Cauchy stresses
		const mat3ds sm = 1.0/J*(F*(Sm*F.transpose())).sym();
		const mat3ds sc = 1.0/J*(F*(Sc*F.transpose())).sym();
		const mat3ds sa = 1.0/J*(F*(Sa*F.transpose())).sym();
		const mat3ds sx = 1.0/J*(F*(Sx*F.transpose())).sym();

		const tens4dmm Ixsx = dyad1mm(I,sx);
		const tens4dmm smxI = dyad1mm(sm,I);
		const tens4dmm scxI = dyad1mm(sc,I);
		const tens4dmm saxI = dyad1mm(sa,I);

		const mat3ds tenr = dyad(F*(Fio*N[0]));						// Fio needed for consistency (from computation of lr)
		const mat3ds tent = dyad(F*(Fio*N[1]));
		const mat3ds tenz = dyad(F*(Fio*N[2]));

		const tens4dmm Ixnrr = dyad1mm(I,tenr);
		const tens4dmm Ixntt = dyad1mm(I,tent);

		// contribution due to constant Cauchy stresses at constituent level
		tens4dmm cfss(0.0);

		sfpro.zero();
		sfpro(0,0) = eigenvec[0]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[0]);
		sfpro(1,1) = eigenvec[1]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[1]);
		sfpro(2,2) = eigenvec[2]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[2]);
		sfpro(0,1) = eigenvec[0]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[1]);
		sfpro(1,2) = eigenvec[1]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[2]);
		sfpro(0,2) = eigenvec[0]*((phim*sNm+phic*sNc+phim*sNa)*eigenvec[2]);

		vec3d Fxeigenvec[3];

		Fxeigenvec[0] = F*eigenvec[0];
		Fxeigenvec[1] = F*eigenvec[1];
		Fxeigenvec[2] = F*eigenvec[2];

		for (int i=0; i<3; i++) {

			mat3ds ten1 = dyad(Fxeigenvec[i]);

			for (int j=0; j<3; j++) {

				double component = sfpro(i,j) / pow(eigenval[i],3) / eigenval[j];

				mat3ds ten2 = dyads(Fxeigenvec[i],Fxeigenvec[j]);

				cfss -= component*dyad1mm(ten2,ten1);

				for (int k=0; k<3; k++) {

					if (k == i) continue;

					mat3ds ten3 = dyads(Fxeigenvec[j],Fxeigenvec[k]);
					mat3ds ten4 = dyads(Fxeigenvec[k],Fxeigenvec[i]);

					component = sfpro(i,j) / eigenval[i] / eigenval[j] / eigenval[k] / (eigenval[i] + eigenval[k]);

					cfss -= component*dyad1mm(ten3,ten4);
				}
			}
		}

		const double dphiRm = phimo*eta*pow(J/Jo*phic/phico,eta-1.0)/(phimo*eta*pow(J/Jo*phic/phico,eta-1.0)+phico);
		const double dphiRc = phico/(phimo*eta*pow(J/Jo*phic/phico,eta-1.0)+phico);

		cfss += dphiRm*(smxI+saxI) + dphiRc*scxI;

		// contribution due to the ratio of vasocontrictors to vasodilators in the active stress
		const tens4dmm saoxnrr = dyad1mm((R*sao*R.transpose()).sym(),tenr);
		const tens4dmm saoxntt = dyad1mm((R*sao*R.transpose()).sym(),tent);

		// 1/J * FoF : [ J * phim * 1/(1.0-exp(-CB*CB)) * (Ui*sao*Ui) x d(1-exp(-Cratio^2))/d(C/2) ] : (Ft)o(Ft)
		const tens4dmm cass = phim * 6.0*Cratio*CS*EPS*pow(rIrIo,-4)*exp(-Cratio*Cratio)/(1.0-exp(-CB*CB)) * (ro/rIo/lt*saoxntt-(ro-rIo)/rIo/lr*saoxnrr);

		// contribution due to change in Cauchy stresses at constituent level (orientation only, for now)
		tens4dmm cpnss(0.0);

		const double scphato = cc*(lcp2-1.0)*exp(dc*(lcp2-1.0)*(lcp2-1.0))*(Gc*Gc);	// constant stress magnitude at constituent level
		const double scnhato = cc*(lcn2-1.0)*exp(dc*(lcn2-1.0)*(lcn2-1.0))*(Gc*Gc);

		const vec3d dNpdta = (N[1]-N[2]*tan(alpha))*pow(1+pow(tan(alpha),2),-1.5);	// d(Np)/d(tan(alpha))
		const vec3d dNndta = (N[1]+N[2]*tan(alpha))*pow(1+pow(tan(alpha),2),-1.5);

		const mat3ds ten1 = 1.0/Jo*dyads(R*(Uo*dNpdta),R*(Uo*Np));					// FoF : (Ui)o(Ui) : d(NpxNp)/d(tan(alpha)), with Jo and Uo needed for consistency (from computation of sco)
		const mat3ds ten2 = 1.0/Jo*dyads(R*(Uo*dNndta),R*(Uo*Nn));

		const mat3ds ten3 = aexp*tan(alpha)*(1.0/(lt*lt)*tent-1.0/(lz*lz)*tenz);		// 2*d(tan(alpha))/d(C) : (Ft)o(Ft)

		cpnss += (phic*betad) * scphato * dyad1mm(ten1,ten3);					// 1/J * FoF : [ J * phicp * scphato * (Ui)o(Ui) : 2*d(NpxNp)/d(C) ] : (Ft)o(Ft)
		cpnss += (phic*betad) * scnhato * dyad1mm(ten2,ten3);

		const tens4dmm cess = tens4dmm(ce);							// ce in tens4dmm form

		css = cess + cfss + cass + cpnss;

		css += 1.0/3.0*(2.0*sx.tr()*IoIss-2.0*Ixsx-ddot(IxIss,css))
			 + svo/(1.0-delta)*(1.0+KsKi*(EPS*pow(rIrIo,-3)-1.0)-KfKi*inflam)*(IxIss-2.0*IoIss)
			 - 3.0*svo/(1.0-delta)*KsKi*EPS*pow(rIrIo,-4)*(ro/rIo/lt*Ixntt-(ro-rIo)/rIo/lr*Ixnrr);
	}

	mat3ds s = 1.0/J*((F*(S*F.transpose()))).sym();
	stress = s;

	pt.m_Iemax = s.dotdot(dyad(F*N[1]))/(F*N[1]).norm2();			// circumferential stress, just for plotting, temporary

	tangent = css;
}
