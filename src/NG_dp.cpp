// Neutron generator (et al) data processor
// Written by Darryl, based on algorithms initially written by Jacques and some inspiration from Cassie

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <ratio>

#ifndef PROCESSOR_H
#include "Processor.h"
#endif
#ifndef CONFIG_H
#include "Config.h"
#endif

using namespace std::chrono;

const float method_versions[NUM_METHODS] = {
	CCM::sf_version,
	DFT::sf_version,
	XSQ::sf_version,
	LAP::sf_version
};

bool g_verbose(false);

int main(int argc, char **argv) {
	cout << "Neutron generator data processor v3_5\n";
	int i(0), i_err_code(0), i_timenow(0), i_datenow(0), i_special(-1), method_id(0), i_pga_check[MAX_CH], i_fast_check[MAX_CH], i_slow_check[MAX_CH], i_XSQ_ndf(0);
	float f_version(0);
	string s_config_file = "\0", s_fileset = "\0";
	config_t config;
	f_header_t f_header;
	char c_filename[64], c_source[12], c_methodname[12], c_chmod[128], c_overwrite(0);
	time_t t_rawtime;
	tm* t_today;
	steady_clock::time_point t_start, t_end;
	duration<double> t_elapsed;
	bool b_update(0), b_checked[NUM_METHODS];
	unique_ptr<TFile> f = nullptr;
	unique_ptr<TTree> tx = nullptr, tc = nullptr, tv = nullptr;
	unique_ptr<Digitizer> digitizer;
	ifstream fin;
	memset(c_source, ' ', sizeof(c_source));
	memset(b_checked, 0, sizeof(b_checked));
	if (argc < 3) {
		cout << "Arguments: -f file [-s source -c config -x special]\n";
		return 0;
	}
	while ((i = getopt(argc, argv, "c:f:s:vx:")) != -1) {
		switch(i) {
			case 'c': s_config_file = optarg;	break;
			case 'f': s_fileset = optarg;		break;
			case 's': c_strcpy(source,optarg);	break;
			case 'v': g_verbose = true;			break;
			case 'x': i_special = atoi(optarg);	break;
			default: cout << "Arguments: -f file [-s source -c config -x special]\n";
				return -1;
	}	}
	if (s_fileset == "\0") {
		cout << "No file specified\n";
		return 0;
	}
	switch(i_special) {
		case -1: break; //cout << "No special options\n"; break; // default
		case 0: cout << "Special samplerate\n"; break; // 500 MSa/s samplerate
		case 1: cout << "Special resolution: 13-bit\n"; break; // 13-bit simulation
		case 2: cout << "Special resolution: 12-bit\n"; break; // 12-bit simulation
		case 3: cout << "Special resolution: 11-bit\n"; break; // 11-bit simulation
		case 4: cout << "Special resolution: 10-bit\n"; break; // 10-bit simulation
		default : cout << "Error: invalid special option specified\n"; return 0;
	}
	memset(&config, 0, sizeof(config_t));
	
	if (i_special == -1) sprintf(c_filename, "%sprodata/%s.root", path, s_fileset.c_str());
	else sprintf(c_filename, "%sprodata/%s_x.root", path, s_fileset.c_str());
	fin.open(filename, ios::in);
	config.already_done =  fin.is_open();
	if (fin.is_open()) fin.close();

	sprintf(c_filename, "%srawdata/%s.dat", path, s_fileset.c_str());
	fin.open(c_filename, ios::in | ios::binary);
	if (!fin.is_open()) {
		for (i = 0; i < MAX_CH; i++) {
			sprintf(c_filename, "%srawdata/%s_%i.dat", path, s_fileset.c_str(), i);
			fin.open(c_filename, ios::in);
			if (fin.is_open()) {
				cout << "Legacy files no longer supported, please run up-converter\n";
				fin.close();
				return 0;
		}	}
		cout << "Error: " << s_fileset << " not found\n";
		return 0;
	}
	GetFileHeader(&fin, &config, &f_header);
	if (config.numEvents == 0) {
		cout << "Error: file header not processed\n";
		return 0;
	}
	
	try {digitizer = unique_ptr<Digitizer>(new Digitizer(f_header.dig_name, i_special));}
	catch (bad_alloc& ba) {
		cout << "Could not instantiate Digitizer class\n";
		return 0;
	} if ( (i_err_code = digitizer->Failed()) ) {
		cout << error_message[i_err_code] << "\n";
		return 0;
	}
	
	if (s_config_file == "\0") s_config_file = "NG_dp_config.cfg";
	cout << "Parsing " << s_config_file << " with settings for " << digitizer->Name() << ": ";
	if ( (i_err_code = ParseConfigFile(s_config_file, &config, digitizer->Name())) ) {
		cout << "failed: " << error_message[i_err_code] << '\n';
		return 0;
	} else cout << "done\n";
	
	cout << "Found " << config.numEvents << " events and " << config.nchan << " channels\n";
	
	time(&t_rawtime);
	t_today = localtime(&t_rawtime);
	i_timenow = (t_today->tm_hour)*100 + t_today->tm_min; // hhmm
	i_datenow = (t_today->tm_year-100)*10000 + (t_today->tm_mon+1)*100 + t_today->tm_mday; // yymmdd
	
	switch (digitizer->ID()) {
		case dt5751 : 
			if (digitizer->Special() == 0) i_XSQ_ndf = min(config.eventlength/2, 225)-3; // length - number of free parameters
			else i_XSQ_ndf = min(config.eventlength, 450)-3;
			break;
		case dt5751des :
			i_XSQ_ndf = min(config.eventlength, 899)-3;
			break;
		case dt5730 :
			i_XSQ_ndf = min(config.eventlength, 225)-3;
			break;
		case v1724 :
		default :
			i_XSQ_ndf = -1;
			break;
	}
	
	if (i_special == -1) sprintf(c_filename, "%sprodata/%s.root", path, s_fileset.c_str());
	else sprintf(c_filename, "%sprodata/%s_x.root", path, s_fileset.c_str());
	try {f = unique_ptr<TFile>(new TFile(filename, "UPDATE"));}
	catch (bad_alloc& ba) {
		cout << "Allocation error\n";
		return 0;
	} if (f->IsZombie()) {
		cout << "Error: could not open root file\n";
		return 0;
	}
	f->cd();
	if (config.already_done) {
		tx.reset((TTree*)f->Get("Tx"));
		if (tx) {
			cout << s_fileset << " already processed with an older version of NG_dp; v3.5 will require the removal of previous results.  Continue <y|n>? ";
			cin >> c_overwrite;
			if (c_overwrite == 'y') {
				f->Delete("*;*");
				config.already_done = false;
			} else {
				f->Close();
				f.reset();
				return 0;
			}
		}
		tx.reset();
	}
	
	if (config.already_done) {
		cout << "Processing already done, checking versions...\n";
		tv = unique_ptr<TTree>((TTree*)f->Get("TV"));
		tc = unique_ptr<TTree>((TTree*)f->Get("TC"));
		tv->SetBranchAddress("MethodID", &method_id);
		tv->SetBranchAddress("Version", &f_version);
		tc->SetBranchAddress("PGA_samples", i_pga_check);
		tc->SetBranchAddress("Fast_window", i_fast_check);
		tc->SetBranchAddress("Slow_window", i_slow_check);
		tc->GetEntry(tc->GetEntries()-1);
		for (i = tv->GetEntries() - 1; i >= 0; i--) { // most recent entry will be last
			tv->GetEntry(i); // so it's better to start from the end rather than the beginning
			b_update = false;
			config.method_done[method_id] = (b_checked[method_id] ? config.method_done[method_id] : true);
			if (config.method_active[method_id] && !b_checked[method_id]) {
				b_checked[method_id] = true;
				config.method_done[method_id] = true;
				b_update |= (f_version < method_versions[method_id]);
				b_update |= ((method_id == CCM_t) && (memcmp(i_pga_check, config.pga_samples, sizeof(i_pga_check)) != 0));
				b_update |= ((method_id == CCM_t) && (memcmp(i_fast_check, config.fastTime, sizeof(i_fast_check)) != 0));
				b_update |= ((method_id == CCM_t) && (memcmp(i_slow_check, config.slowTime, sizeof(i_slow_check)) != 0));
				if (b_update) {
					cout << "Method " <<  method_names[method_id] << " will be updated\n";
					sprintf(c_filename, "T%i;*", method_id);
					f->Delete(c_filename);
					config.method_done[method_id] = false;
				} else {
					cout << "Method " << method_names[method_id] << " does not need updating, reprocess anyway <y|n>? ";
					cin >> c_overwrite;
					if (c_overwrite == 'y') {
						sprintf(c_filename, "T%i;*", method_id);
						f->Delete(c_filename);
						config.method_done[method_id] = false;
					} else config.method_active[method_id] = false;
			}	}
		}
		if (memchr(config.method_active, 1, NUM_METHODS) == NULL) cout << "No processing methods activated\n";
		tv->SetBranchAddress("Date", &i_datenow);
		tv->SetBranchAddress("Time", &i_timenow);
		tv->SetBranchAddress("MethodName", c_methodname);
		tc->SetBranchAddress("PGA_samples", config.pga_samples);
		tc->SetBranchAddress("Fast_window", config.fastTime);
		tc->SetBranchAddress("Slow_window", config.slowTime);
		for (i = 0; i < NUM_METHODS; i++) if (config.method_active[i]) {
			method_id = i;
			f_version = method_versions[i];
			strcpy(c_methodname, method_names[i]);
			tv->Fill();
			if (i == CCM_t) tc->Fill();
		}
		tc->Write("",TObject::kOverwrite);
		tv->Write("",TObject::kOverwrite);
	} else {
		cout << "Creating config trees\n";
		try {
			tx = unique_ptr<TTree>(new TTree("TI", "Info"));
			tc = unique_ptr<TTree>(new TTree("TC","CCM,PGA info"));
			tv = unique_ptr<TTree>(new TTree("TV", "Versions"));
		} catch (bad_alloc& ba) {
			cout << "Could not create config trees\n";
			f->Close();
			return 0;
		} if ((tx->IsZombie()) || (tc->IsZombie()) || (tv->IsZombie())) {
			cout << "Could not create config trees\n";
			f->Close();
			return 0;
		}
		tx->Branch("Digitizer", f_header.dig_name, "name[12]/B");
		tx->Branch("Source", c_source, "source[12]/B");
		tx->Branch("ChannelMask", &config.mask, "mask/s");
		tx->Branch("TriggerThreshold", f_header.threshold, "threshold[8]/i");
		tx->Branch("DC_offset", f_header.dc_off, "dc_off[8]/i"); // the numbers in this tree
		tx->Branch("Posttrigger", &config.trig_post, "tri_post/I"); // don't change, so it's only
		tx->Branch("Eventlength", &config.eventlength, "ev_len/I"); // written out once
		tx->Branch("Chisquared_NDF", &i_XSQ_ndf, "ndf/I");
		if (special != -1) tx->Branch("Special", &i_special, "special/I");
		
		tv->Branch("MethodName", c_methodname, "codename[12]/B");
		tv->Branch("MethodID", &method_id, "codeid/I");
		tv->Branch("Date", &i_datenow, "date/I"); // this tree holds version info
		tv->Branch("Time", &i_timenow, "time/I");
		tv->Branch("Version", &f_version, "version/F");
		
		tc->Branch("PGA_samples", config.pga_samples, "pga[8]/I");
		tc->Branch("Fast_window", config.fastTime, "fast[8]/I");
		tc->Branch("Slow_window", config.slowTime, "slow[8]/I");
		for (i = 0; i < NUM_METHODS; i++) if (config.method_active[i]) {
			strcpy(c_methodname, method_names[i]);
			f_version = method_versions[i];
			method_id = i;
			tv->Fill();
		}
		tx->Fill();
		tc->Fill();
		tx->Write();
		tv->Write();
		tc->Write();
	}
	tx.reset();
	tc.reset();
	tv.reset();
	
	if ((digitizer->ID() > 2) && (config.method_active[XSQ_t])) {
		cout << "Digitizer " << digitizer->Name() << " is not compatible with Chisquared method\n";
		config.method_active[XSQ_t] = false;
	}
	if (memchr(config.method_active, 1, NUM_METHODS) == NULL) {
		f->Close();
		f.reset();
		digitizer.reset();
		return 0;
	} else {
		cout << "Processing methods activated: ";
		for (i = 0; i < NUM_METHODS; i++) if (config.method_active[i]) cout << method_names[i] << " ";
		cout << '\n';
	}
	
	t_start = steady_clock::now();
	if ( (i_err_code = Processor(&config, &fin, f.release(), digitizer.release())) ) cout << error_message[err_code] << '\n';
	t_end = steady_clock::now();
	t_elapsed = duration_cast<duration<double>>(t_end-t_start);
	f.reset();
	digitizer.reset();
	if (i_special == -1) sprintf(c_filename, "%sprodata/%s.root", path, s_fileset.c_str());
	else sprintf(c_filename, "%sprodata/%s_x.root", path, s_fileset.c_str());
	sprintf(c_chmod,"chmod g+w %s", c_filename);
	system(c_chmod);
	cout << "Total time elapsed: " << t_elapsed.count() << "sec\n";
	return 0;
}
