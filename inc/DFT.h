#ifndef DFT_H
#define DFT_H

#ifndef METHOD_H
#include "Method.h"
#endif

class DFT : public Method {
	private:
		const int ciOrder; // 3 non-zero
		double dScalefactor;
		unique_ptr<double[]> dCos;
		unique_ptr<double[]> dSin;

		static unique_ptr<TTree> tree;
		static int siHowMany;
		static bool sbInitialized;
		
		static double sdMagnitude[8][4]; // order = 3
		static double sdPhase[8][4]; // but we want 0th as well

		
	public:
		DFT();
		DFT(int ch, int len, shared_ptr<Digitizer> digitizer);
		virtual ~DFT();
		virtual void Analyze();
		virtual void SetParameters(void* val, int which, shared_ptr<Digitizer> digitizer) {}
		static void root_fill() {DFT::tree->Fill();}
		static void root_init(TTree* tree_in);
		static void root_deinit() {DFT::tree.reset();} // friending is handled after the fact, writing by the TFile
		static int HowMany() {return DFT::siHowMany;}
		static float sfVersion;
};

#endif // DFT_H
