/***************************************************************************
 *   Copyright (C) 2005, 2006 by Jonathan Duddington                       *
 *   jsd@clara.co.uk                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "wx/wx.h"
#include "wx/wfstream.h"
#include "wx/dir.h"
#include "wx/filename.h"

#include "main.h"
#include "speech.h"
#include "voice.h"
#include "spect.h"
#include "options.h"
#include "phoneme.h"
#include "synthesize.h"


#define tNUMBER   1
#define tSTRING   2
#define tPHONEMEMNEM  3
#define tKEYWORD  4
#define tSIGNEDNUMBER 5

#define tPHONEMESTART 1
#define tEND      2
#define tSPECT    3
#define tWAVE   4
#define tSTARTTYPE 5
#define tENDTYPE  6
#define tBEFORE   7
#define tAFTER    8
#define tTONESPEC 9
#define tLENGTHMOD 11
#define tLENGTH   12
#define tLONGLENGTH 13
#define tOLDNAME  14
#define tREDUCETO 15
#define tFIXEDCODE 16
#define tBEFOREVOWEL 17
#define tBEFOREVOWELPAUSE 18
#define tBEFORENOTVOWEL 19
#define tLINKOUT  20
#define tSWITCHVOICING  21
#define tVOWELIN  22
#define tVOWELOUT 23
#define tPHONEMENUMBER 29
#define tPHONEMETABLE  30
#define tINCLUDE  31


extern void MakeVowelLists(void);
extern int CompileDictionary(const char *dsource, const char *dict_name, FILE *log, char *fname);
extern char voice_name[];

static int markers_used[8];

#define N_USED_BY  12
typedef struct {
	void *link;
	int value;
	short n_uses;
	short n_used_by;
	unsigned char used_by[N_USED_BY];
	char string[1];
} REF_HASH_TAB;


class Compile
{//==========
public:
	Compile();
	void CPhonemeTab(const char *path);

private:
	int CPhoneme();
	void StartPhonemeTable(const char *name);
	void EndPhonemeTable();
	void CPhonemeFiles(char *path_source);
	int NextItem(int type);
	void UngetItem();
	void Error(const char *msg);
	void Error(const char *msg, const char *msg2);
	void Summary(FILE *f);
	void SummarySpect(FILE *f,unsigned int value);
	void SummarySpectList(FILE *f,int ref,const char *label);
	const char *PhonemeMnem(int phcode);
	void WritePhonemeTable();
	void Report();
	
	int LookupPhoneme(const char *string, int declare);
	void ReservePhCode(const char *string, int value);
	const char *GetKeyMnem(int value);
	int LoadSpect(const char *path, int control);
	int LoadWavefile(FILE *f, const char *fname);
	int LoadEnvelope(FILE *f);
	int LoadDataFile(const char *path, int control);
	int AddSpect(int phcode, int *list, int control);
	void AddSpectList(int *list, int control);
	void AddEnvelope(int *list);
	void VowelTransition(int which, unsigned int *trans);


	FILE *f_in;
	FILE *f_spects;
	FILE *f_samples;
	FILE *f_errors;
	FILE *f_phindex;
	FILE *f_phdata;
	int f_in_displ;
	int f_in_linenum;

	PHONEME_TAB *ph;

	int linenum;
	int item_type;
	char item_string[256];
	char phoneme_tab_name[N_PHONEME_TAB_NAME];
	unsigned int vowel_in[2];
	unsigned int vowel_out[2];

	int n_phcodes_list[N_PHONEME_TABS];
	PHONEME_TAB_LIST phoneme_tab_list2[N_PHONEME_TABS];
	PHONEME_TAB *phoneme_tab2;
	int n_phoneme_tabs;
	
	
	int n_phcodes;
	
	int count_references;
	int duplicate_references;
	int count_frames;
	int error_count;
	
	REF_HASH_TAB *ref_hash_tab[256];
	
	typedef struct {
		FILE *file;
		int  linenum;
	} STACK;
	
	#define N_STACK  12
	int stack_ix;
	STACK stack[N_STACK];

};

char path_source[80];

Compile *compile;

typedef struct {
	const char *mnem;
	int data;
} keywtab_t;

static keywtab_t keywords[] = {
	{"vowel",     0x1000000+phVOWEL},
	{"liquid",    0x1000000+phLIQUID},
	{"pause",     0x1000000+phPAUSE},
	{"stress",    0x1000000+phSTRESS},
	{"virtual",   0x1000000+phVIRTUAL},

	{"fricative", 0x1000000+phFRICATIVE},
	{"vstop",     0x1000000+phVSTOP},
	{"vfricative",0x1000000+phVFRICATIVE},

	// type of consonant
	{"stop",      0x1000000+phSTOP},
	{"frc",       0x1000000+phFRICATIVE},
	{"nasal",     0x1000000+phNASAL},
	{"flp",       0x1000000+phVSTOP},
	{"afr",       0x1000000+phSTOP},      // treat as stop
	{"apr",       0x1000000+phFRICATIVE}, // [h] voiceless approximant


	// keywords
	{"phonemenumber", 29},
	{"phonemetable", 30},
	{"include", 31},

	{"phoneme",  1},
	{"endphoneme",  2},
	{"formants", 3},
	{"wave",     4},
	{"starttype",5},
	{"endtype",  6},
	{"before",   7},
	{"after",    8},
	{"tone",     9},
	{"lengthmod",11},
	{"length",   12},
	{"longlength", 13},
	{"reduceto", 15},
	{"beforevowel", 17},
	{"beforevowelpause", 18},
	{"beforenotvowel",19},
	{"linkout",20},
	{"switchvoicing",21},
	{"vowelin",22},
	{"vowelout",23},

	// flags
	{"wavef",      0x2000000+phWAVE},
	{"unstressed", 0x2000000+phUNSTRESSED},
	{"fortis",     0x2000000+phFORTIS},
	{"sibilant",   0x2000000+phSIBILANT},
	{"nolink",     0x2000000+phNOLINK},
	{"trill",      0x2000000+phTRILL},
	{"vowel2",     0x2000000+phVOWEL2},
	{"palatal",    0x2000000+phPALATAL},
	{"long",       0x2000000+phLONG},

	// voiced / unvoiced
	{"vcd",	0x2000000+phVOICED},
	{"vls",	0x2000000+phFORTIS},

	// place of articulation
	{"blb",	0x2010000},
	{"lbd",	0x2020000},
	{"dnt",	0x2030000},
	{"alv",	0x2040000},
	{"rfx",	0x2050000},
	{"pla",	0x2060000},
	{"pal",	0x2070000},
	{"vel",	0x2080000},
	{"lbv",	0x2090000},
	{"uvl",	0x20a0000},
	{"phr",	0x20b0000},
	{"glt",	0x20c0000},

	// vowel transition attributes
	{"len=",	0x3000001},
	{"rms=",	0x3000002},
	{"f1=",	0x3000003},
	{"f2=",	0x3000004},
	{"f3=",	0x3000005},
	{"brk",  0x3000006},
	{"rate", 0x3000007},
	{"glstop", 0x3000008},
	{"lenadd", 0x3000009},
	{"", -1}
};


extern const char *WordToString(unsigned int word);
extern void strncpy0(char *to,const char *from, int size);



static unsigned int StringToWord(const char *string)
{//=================================================
// Pack 4 characters into a word
	int  ix;
	unsigned char c;
	unsigned int word;

	word = 0;
	for(ix=0; ix<4; ix++)
	{
		if(string[ix]==0) break;
		c = string[ix];
		word |= (c << (ix*8));
	}
	return(word);
}


static FILE *fopen_log(FILE *f_log, const char *fname,const char *access)
{//==================================================
// performs fopen, but produces error message to f_log if it fails
	FILE *f;

	if((f = fopen(fname,access)) == NULL)
	{
		if(f_log != NULL)
			fprintf(f_log,"Can't access (%s) file '%s'\n",access,fname);
	}
	return(f);
}


int Hash8(const char *string)
//===========================
/* Generate a hash code from the specified string */
{
   int  c;
	int  chars=0;
   int  hash=0;

   while((c = *string++) != 0)
   {
      c = tolower(c) - 'a';
      hash = hash * 8 + c;
      hash = (hash & 0x1ff) ^ (hash >> 8);    /* exclusive or */
		chars++;
   }

   return((hash+chars) & 0xff);
}   //  end of Hash8





Compile::Compile()
{//===============

	linenum = 0;
}


void Compile::Error(const char *msg)
{//=================================
	fprintf(f_errors,"%3d: %s\n",linenum,msg);
	error_count++;
}

void Compile::Error(const char *msg, const char *msg2)
{//=================================
	fprintf(f_errors,"%3d: %s: '%s'\n",linenum,msg,msg2);
	error_count++;
}



const char *Compile::PhonemeMnem(int phcode)
{//==========================================
	if((phcode >= 0) && (phcode < N_PHONEME_TAB))
		return(WordToString(phoneme_tab2[phcode].mnemonic));
	else
		return("???");
}


const char *Compile::GetKeyMnem(int value)
{//=======================================
	keywtab_t *p;

	p = keywords;

	while(p->data != -1)
	{
		if(p->data == value)
			return(p->mnem);
		p++;
	}
	return("???");
}


void Compile::ReservePhCode(const char *string, int value)
{//=======================================================
// Use a fixed number for this phoneme. Must be used before any
// phonemes are declared
	unsigned int word;

	word = StringToWord(string);

	phoneme_tab2[value].mnemonic = word;
	phoneme_tab2[value].code = value;
	if(n_phcodes <= value)
		n_phcodes = value+1;
}


int Compile::LookupPhoneme(const char *string, int control)
{//========================================================
// control = 0   explicit declaration
// control = 1   declare phoneme if not found
// control = 2   start looking after control & stress phonemes

	int  ix;
	int  start;
	int  use;
	unsigned int word;

	if(strcmp(string,"NULL")==0)
		return(1);

	ix = strlen(string);
	if((ix==0) || (ix> 4))
	{
		Error("Bad phoneme",string);
	}
	word = StringToWord(string);

	// don't use phoneme number 0, reserved for string terminator
	start = 1;
	
	if(control==2)
		start = 8;   // don't look for control and stress phonemes (allows these characters to be
		             // used for other purposes)

	use = 0;
	for(ix=start; ix<n_phcodes; ix++)
	{
		if(phoneme_tab2[ix].mnemonic == word)
			return(ix);

		if((use==0) && (phoneme_tab2[ix].mnemonic == 0))
		{
			use = ix;
		}
	}

//	if(control != 1) return(-1);  // not found

	if(use == 0)
	{
		if(n_phcodes >= N_PHONEME_TAB-1)
			return(-1);   // phoneme table is full
		use = n_phcodes++;
	}

	// add this phoneme to the phoneme table
	phoneme_tab2[use].mnemonic = word;
	phoneme_tab2[use].type = phINVALID;
	phoneme_tab2[use].spect = linenum;  // for error report if the phoneme remains undeclared
	return(use);
}  //  end of Compile::LookupPhoneme



int Compile::NextItem(int type)
{//============================
	int  acc;
	char  c=0;
	char c2;
	int  ix;
	int  sign;
	int  keyword;
	keywtab_t *p;
	
	f_in_displ = ftell(f_in);
	f_in_linenum = linenum;
		
	while(!feof(f_in))
	{
		c = fgetc(f_in);
		if(c=='/')
		{
			if((c2 = fgetc(f_in))=='/')
			{
				// comment, ignore to end of line
				while(!feof(f_in) && ((c=fgetc(f_in)) != '\n'));
			}
			else
			{
				ungetc(c2,f_in);
			}
		}
		if(c == '\n') linenum++;
		if(!isspace(c))
		{
			break;
		}
	}
	if(feof(f_in))
		return(tEND);

	if((type == tNUMBER) || (type == tSIGNEDNUMBER))
	{
		acc = 0;
		sign = 1;
		if((c == '-') && (type == tSIGNEDNUMBER))
		{
			sign = -1;
			c = fgetc(f_in);
		}
		if(!isdigit(c))
			Error("Expected number");
		while(isdigit(c))
		{
			acc *= 10;
			acc += (c - '0');
			c = fgetc(f_in);
		}
		ungetc(c,f_in);
		return(acc * sign);
	}

	ix = 0;
	while(!feof(f_in) && !isspace(c))
	{
		item_string[ix++] = c;
		c = fgetc(f_in);
		if(item_string[ix-1] == '=')
			break;
	}
	ungetc(c,f_in);
	item_string[ix] = 0;

	if(feof(f_in)) return(-1);

	keyword = -1;

	if(type == tSTRING)
	{
		return(0);
	}
	if(type == tKEYWORD)
	{
		p = keywords;
		while(p->data != -1)
		{
			if(strcmp(item_string,p->mnem)==0)
			{
				item_type = p->data >> 24;
				return(p->data & 0xffffff);
			}
			p++;
		}
		item_type = -1;
		return(-1);   // keyword not found
	}
	if(type == tPHONEMEMNEM)
	{
		return(LookupPhoneme(item_string,2));
	}
	return(-1);
}  //  end of Compile::NextItem


void Compile::UngetItem()
{//======================
	fseek(f_in,f_in_displ,SEEK_SET);
	linenum = f_in_linenum;
}  //  end of Compile::UngetItem



int Compile::LoadSpect(const char *path, int control)
{//=================================================
	SpectSeq *spectseq;
	int peak;
	int displ;
	int frame;
	int ix;
	int x;
	int rms;
	float total;
	float pkheight;
	SpectFrame *fr;
	wxString path_sep = _T("/");

	SPECT_SEQ seq_out;

	// create SpectSeq and import data
	spectseq = new SpectSeq;
	if(spectseq == NULL)
	{
		Error("Failed to create SpectSeq");
		return(0);
	}

	wxString filename = path_phsource + path_sep + wxString(path,wxConvLocal);
	wxFileInputStream stream(filename);
	
	if(stream.Ok() == FALSE)
	{
		Error("Failed to open",path);
		return(0);
	}
	spectseq->Load(stream);

	displ = ftell(f_spects);

	seq_out.n_frames=0;
	seq_out.flags=0;
	seq_out.length=0;

	total = 0;	
	for(frame=0; frame < spectseq->numframes; frame++)
	{

#ifdef deleted
for(ix=0; ix<8; ix++)
{
	// find which files have certain markers set
	if(spectseq->frames[frame]->markers & (1<<ix))
	{
		markers_used[ix]++;
		if((ix==3) || (ix==4))
		{
			fprintf(f_errors,"Marker %d: %s\n",ix,path);
		}
	}
}
#endif

		if(spectseq->frames[frame]->keyframe)
		{
			seq_out.n_frames++;
			if(frame > 0)
				total += spectseq->frames[frame-1]->length;
		}	
	}
	seq_out.length = int(total);

	if((control & 1) && (spectseq->numframes > 2))
	{
		// set a marker flag for the second frame of a vowel
		spectseq->frames[1]->markers |= FRFLAG_VOWEL_CENTRE;
	}

	ix = 0;
	for(frame=0; frame < spectseq->numframes; frame++)
	{
		fr = spectseq->frames[frame];
		
		if(fr->keyframe)
		{
			x = int(fr->length + 0.5);  // round to nearest mS
			if(x > 255) x = 255;
			seq_out.frame[ix].length = x;

			seq_out.frame[ix].flags = fr->markers;
			rms = int(fr->GetRms(spectseq->amplitude));
			if(rms > 255) rms = 255;
			seq_out.frame[ix].rms = rms;

			if(ix == (seq_out.n_frames-1))
				seq_out.frame[ix].length = 0;    // give last frame zero length
				
			// write: peak data
			count_frames++;
			for(peak=0; peak<N_PEAKS; peak++)
			{
				seq_out.frame[ix].ffreq[peak] = fr->peaks[peak].pkfreq;
				pkheight = spectseq->amplitude * fr->amp_adjust * fr->peaks[peak].pkheight;
				pkheight = pkheight/640000;
				if(pkheight > 255) pkheight = 255;
				seq_out.frame[ix].fheight[peak] = int(pkheight);

				if(peak < 6)
				{
					x =  fr->peaks[peak].pkwidth/4;
					if(x > 255) x = 255;
					seq_out.frame[ix].fwidth[peak] = x;

					x =  fr->peaks[peak].pkright/4;
					if(x > 255) x = 255;
					seq_out.frame[ix].fright[peak] = x;
				}
			}

#ifdef LOG_DETAIL
fprintf(f_errors,"Frame %d  %3dmS  rms=%3d  flags=%2d  pk=%4d %4d %4d",ix,seq_out.frame[ix].length,
	seq_out.frame[ix].rms,seq_out.frame[ix].flags,
	seq_out.frame[ix].peaks[1].pkfreq,seq_out.frame[ix].peaks[2].pkfreq,seq_out.frame[ix].peaks[3].pkfreq);

	if(fr->markers != 0)
	{
		fprintf(f_errors," [%x]",fr->markers);
	}
	fputc('\n',f_errors);
#endif
	
			ix++;
		}
	}
	
	ix = (char *)(&seq_out.frame[seq_out.n_frames]) - (char *)(&seq_out);
	ix = (ix+3) & 0xfffc;   // round up to multiple of 4 bytes

	fwrite(&seq_out,ix,1,f_spects);
	
	delete spectseq;
	return(displ);
}  //  end of Compile::LoadSpect



int Compile::LoadWavefile(FILE *f, const char *fname)
{//==================================================
	int displ;
	char c1, c2;
	int sample;
	int sample2;
	float x;
	int max = 0;
	int length;
	int *pw;
	int resample_wav = 0;
	char fname_temp[100];
	int scale_factor=0;
	char wave_hdr[40];
	char command[200];

	fread(wave_hdr,40,1,f);  // skip past the WAV header, up to before "data length"
	pw = (int *)(&wave_hdr[24]);

	if((pw[0] != samplerate) || (pw[1] != samplerate*2))
	{
		sprintf(fname_temp,"%s.wav",tmpnam(NULL));
		sprintf(command,"sox \"%s\" -r %d -c 1 -w  %s polyphase\n",fname,samplerate,fname_temp);
		if(system(command) < 0)
		{
			Error("Failed to resample: ",command);
			return(0);
		}

		f = fopen(fname_temp,"r");
		if(f == NULL)
		{
			Error("Can't read temp file: ",fname_temp);
			return(0);
		}
		resample_wav = 1;
		fread(wave_hdr,40,1,f);  // skip past the WAV header, up to before "data length"
	}

	displ = ftell(f_spects);

	// data contains:  4 bytes of length (n_samples * 2), followed by 2-byte samples (lsb byte first)
	fread(&length,4,1,f);

	while(!feof(f))
	{
		c1 = fgetc(f);
		c2 = fgetc(f);
		if(feof(f)) break;

		sample = (c1 & 0xff)+ (c2 * 256);

		if(sample > max)
			max = sample;
		else
		if(sample < -max)
			max = -sample;

	}
	if(max > 1)
	{
		scale_factor = (max / 127) + 1;
	}
	else
		scale_factor = 0;

//fprintf(f_errors," sample len=%d max=%4x shift=%d\n",length,max,scale_factor);

#define MIN_FACTOR   6
	if(scale_factor > MIN_FACTOR)
	{
		length = length/2 + (scale_factor << 16);
	}

	fwrite(&length,4,1,f_spects);
	fseek(f,44,SEEK_SET);

	while(!feof(f))
	{
		c1 = fgetc(f);
		c2 = fgetc(f);
		sample = (c1 & 0xff)+ (c2 * 256);

		if(feof(f)) break;

      if(scale_factor <= MIN_FACTOR)
		{
			fputc(sample & 0xff,f_spects);
			fputc(sample >> 8,f_spects);
		}
		else
		{
			x = (float(sample) / scale_factor) + 0.5;
			sample2= int(x);
			if(sample2 > 127)
				sample2 = 127;
			if(sample2 < -128)
				sample2 = -128;
			fputc(sample2,f_spects);
		}
	}

	length = ftell(f_spects);
	while((length & 3) != 0)
	{
		// pad to a multiple of 4 bytes
		fputc(0,f_spects);
		length++;
	}

	if(resample_wav != 0)
	{
		fclose(f);
		remove(fname_temp);
	}
	return(displ | 0x800000);  // set bit 23 to indicate a wave file rather than a spectrum
}  //  end of Compile::LoadWavefile



int Compile::LoadEnvelope(FILE *f)
{//===============================
	int displ;
	char buf[128];

	displ = ftell(f_spects);

	fseek(f,12,SEEK_SET);
	fread(buf,1,128,f);
	fwrite(buf,1,128,f_spects);

	return(displ);
}



int Compile::LoadDataFile(const char *path, int control)
{//====================================================
	// load spectrum sequence or sample data from a file.
	// return index into spect or sample data area. bit 23=1 if a sample

	FILE *f;
	int id;
	int ix;
	int hash;
	REF_HASH_TAB *p, *p2;
	char buf[256];

	count_references++;

	hash = Hash8(path);
	p = ref_hash_tab[hash];
	while(p != NULL)
	{
		if(strcmp(path,p->string)==0)
		{
			int found = 0;

			duplicate_references++;
			p->n_uses++;

			// add the current phoneme table to a list of users of this data file, if not already listed
			for(ix=0; (ix < p->n_used_by) && (ix < N_USED_BY); ix++)
			{
				if(p->used_by[ix] == n_phoneme_tabs)
				{
					found = 1;
					break;
				}
			}
			if(found == 0)
			{
				if(ix < N_USED_BY)
				{
					p->used_by[ix] = n_phoneme_tabs;
				}
				p->n_used_by++;
			}

			return(p->value);    // already loaded this data
		}
		p = (REF_HASH_TAB *)p->link;
	}

	sprintf(buf,"%s%s",path_source,path);

	if(strcmp(path,"NULL")==0)
		return(0);
	if(strcmp(path,"DFT")==0)
		return(1);

	if((f = fopen(buf,"rb")) == NULL)
	{
		sprintf(buf,"%s%s.wav",path_source,path);
		if((f = fopen(buf,"rb")) == NULL)
		{
			Error("Can't read file",path);
			return(0);
		}
	}

	fread(&id,1,4,f);
	rewind(f);

	if(id == 0x43455053)
		ix = LoadSpect(path, control);
	else
	if(id == 0x46464952)
		ix = LoadWavefile(f,buf);
	else
	if(id == 0x43544950)
		ix = LoadEnvelope(f);
	else
	{
		Error("File not SPEC or RIFF",path);
		ix = -1;
	}
	fclose(f);

	// add this item to the hash table
	p = ref_hash_tab[hash];
	p2 = (REF_HASH_TAB *)malloc(sizeof(REF_HASH_TAB)+strlen(path)+1);
	p2->value = ix;
	p2->n_uses = 1;
	p2->n_used_by = 1;
	strcpy(p2->string,path);
	p2->used_by[0] = n_phoneme_tabs;
	p2->link = (char *)p;
	ref_hash_tab[hash] = p2;

	return(ix);
}   //  end of Compile::LoadDataFile



void Compile::AddEnvelope(int *list)
{//================================
	NextItem(tSTRING);
	*list++ = LoadDataFile(item_string,0);
	list[0] = 0;
}


int Compile::AddSpect(int phcode, int *list, int control)
{//=====================================================
	int sign;
	int sign2;
	int all_digits;
	char *p;
	char *p2;
	int spect;
	int value;
	int count = 0;
	int v_in = vowel_in[0];
	int v_out = vowel_out[0];

	sign = 0;
	all_digits=0;
	p = p2 = item_string;
	for(;;)
	{
		if((*p == 0) || (*p == '+') || (((*p == '-') || (*p == '%')) && isdigit(p[1])))
		{
			sign2 = *p;
			*p = 0;
			
			if(all_digits)
			{
				value = atoi(p2);
				if(sign == '+')
					*list++ = 1 + (value<<8);
				else
				if(sign == '-')
					*list++ = 2 + (value<<8);
				else
				{
					value = (value * 32)/100;  // convert from % to 1/32s
					*list++ = 3 + (value<<8);
				}
				count++;
			}
			else
			{
				spect = LoadDataFile(p2, control);
				if(spect != -1)
				{
					*list++ = phcode + (spect<<8);
					count++;


					// vowel formant transitions specified ?
					if(v_in != 0)
					{
						*list++ = 0x4 + (vowel_in[0] << 8);
						*list++ = vowel_in[1];
						v_in = 0;
						count+=2;
					}
					if(v_out != 0)
					{
						*list++ = 0x5 + (vowel_out[0] << 8);
						*list++ = vowel_out[1];
						v_out = 0;
						count+=2;
					}

				}
				phcode = 0;
			}
			
			if((sign = sign2)==0) break;
			p2 = p+1;
			all_digits=1;
		}
		else
		{
			if(!isdigit(*p))
				all_digits=0;
		}
		p++;
	}
	*list = 0;
	return(count);
}  // end of  AddSpect



void Compile::AddSpectList(int *list, int control)
{//==============================================
	int phcode;
	int key;

	while(*list != 0) list++;   // find end of the list
	
	for(;;)
	{
		key = NextItem(tKEYWORD);
		UngetItem();
		if(key != -1)
			break;
		
		phcode = NextItem(tPHONEMEMNEM);
		
		if(phcode == -1)
			Error("Undeclared phoneme",item_string);

		if(NextItem(tSTRING) == -1)
			break;

		list += AddSpect(phcode, list, control);
	}

	*list++ = 0;
}  //  end of Compile::AddSpectList



static int Range(int value, int divide, int min, int max)
{//======================================================
	if(value < 0)
		value -= divide/2;
	else
		value += divide/2;
	value = value / divide;

	if(value > max)
		value = max;
	if(value < min)
		value = min;
	return(value - min);
}


void Compile::VowelTransition(int which, unsigned int *trans)
{//==========================================================
// Compile a vowel transition
	int key;
	int len=0;
	int rms=0;
	int f1=0;
	int f2=0;
	int f2_min=0;
	int f2_max=0;
	int f3_adj=0;
	int f3_amp=0;
	int flags=0;

	if(which==1)
	{
		len = 50 / 2;  // defaults for transition into vowel
		rms = 25 / 2;
	}
	else
	if(which==2)
	{
		len = 36 / 2;  // defaults for transition out of vowel
		rms = 16 / 2;
	}

	for(;;)
	{
		key = NextItem(tKEYWORD);
		if(item_type != 3)
		{
			UngetItem();
			break;
		}

		switch(key & 0xf)
		{
		case 1:
			len = Range(NextItem(tNUMBER), 2, 0, 63) & 0x3f;
			flags |= 1;
			break;
		case 2:
			rms = Range(NextItem(tNUMBER), 2, 0, 63) & 0x3f;
			flags |= 1;
			break;
		case 3:
			f1 = NextItem(tNUMBER);
			break;
		case 4:
			f2 = Range(NextItem(tNUMBER), 50, 0, 63) & 0x3f;
			f2_min = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			f2_max = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			break;
		case 5:
			f3_adj = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			f3_amp = Range(NextItem(tNUMBER), 8, 0, 15) & 0x1f;
			break;
		case 6:
			flags |= 2;   // break
			break;
		case 7:
			flags |= 4;   // rate
			break;
		case 8:
			flags |= 8;   // glstop
			break;
		case 9:
			flags |= 16;  // lenadd
			break;
		}
	}
	trans[0] = len + (rms << 6) + (flags << 12) + 0x80000000;
	trans[1] =  f2 + (f2_min << 6) + (f2_max << 11) + (f3_adj << 16) + (f3_amp << 21) + (f1 << 26);
}  // end of VowelTransition



int Compile::CPhoneme()
{//====================
	int phcode;
	int phindex;
	int value;
	int item;
	int finish=0;
	int *intp;
	int before_tab[N_PHONEME_TAB];
	int after_tab[N_PHONEME_TAB];
	int default_spect[N_PHONEME_TAB];

	NextItem(tSTRING);
	phindex = LookupPhoneme(item_string,1);    // declare phoneme if not already there
	if(phindex == -1) return(0);

	ph = &phoneme_tab2[phindex];
	memset(ph,0,sizeof(PHONEME_TAB));
	ph->mnemonic = StringToWord(item_string);
	ph->type = 0xff;   // undefined
	ph->code = phindex;
	before_tab[0] = 0;
	after_tab[0] = 0;
	default_spect[0] = 0;
	vowel_in[0] = 0;
	vowel_in[1] = 0;
	vowel_out[0] = 0;
	vowel_out[1] = 0;

	while(!finish)
	{
		item = NextItem(tKEYWORD);

		if(item_type==2)
			ph->phflags |= item & 0xffffff;
		else
		if(item_type==1)
		{
			if(ph->type != 0xff)
			{
				Error("Phoneme type already set",item_string);
			}
			ph->type = item & 0xff;
		}
		else
		switch(item)
		{
		case tLENGTHMOD:
			ph->length_mod = NextItem(tNUMBER);
			break;

		case tLENGTH:
			ph->std_length = NextItem(tNUMBER);
			break;

		case tWAVE:
			ph->phflags |= phWAVE;           // drop through to tSPECT
		case tSPECT:
			if(NextItem(tSTRING) == -1)
				Error("Syntax error",item_string);
			else
			{
				if(ph->type == phVOWEL)
					AddSpect(phonPAUSE,default_spect,1);
				else
					AddSpect(phonPAUSE,default_spect,0);
			}
			break;

		case tSTARTTYPE:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->start_type = phcode;
			break;

		case tENDTYPE:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->end_type = phcode;
			break;

		case tTONESPEC:
			ph->start_type = NextItem(tNUMBER);   // tone's min pitch (range 0-50)
			ph->end_type = NextItem(tNUMBER);     // tone's max pitch (range 0-50)
			if(ph->start_type > ph->end_type)
			{
				// ensure pitch1 < pitch2
				value = ph->start_type;
				ph->start_type = ph->end_type;
				ph->end_type = value;
			}
			AddEnvelope(default_spect);       // envelope for pitch change,  rise or fall or fall/rise, etc
			AddEnvelope(after_tab);           // envelope for amplitude change
			break;
			
		case tREDUCETO:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->reduce_to = phcode;
			value = NextItem(tNUMBER);
			ph->phflags = (ph->phflags & 0xfffffff) + (value << 28);
			break;

		case tBEFORENOTVOWEL:
			ph->phflags |= phBEFORENOTVOWEL;  // and drop through to tBEFOREVOWEL
		case tBEFOREVOWELPAUSE:
			ph->phflags |= phBEFOREVOWELPAUSE;
		case tBEFOREVOWEL:
			if((phcode = NextItem(tPHONEMEMNEM)) == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->alternative_ph = phcode;
			break;

		case tSWITCHVOICING:
			ph->phflags |= phSWITCHVOICING;
			if((phcode = NextItem(tPHONEMEMNEM)) == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->alternative_ph = phcode;
			break;

		case tLINKOUT:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->link_out = phcode;
			break;

		case tBEFORE:
			AddSpectList(before_tab,0);
			break;
			
		case tAFTER:
			AddSpectList(after_tab,0);
			break;

		case tEND:
			finish = 1;
			break;

		case tVOWELIN:
			VowelTransition(1,vowel_in);
			break;

		case tVOWELOUT:
			VowelTransition(2,vowel_out);
			break;

		default:
			Error("Syntax error",item_string);
			break;
		}
	}
	// write out indices
	if(before_tab[0] != 0)
	{
		ph->before = ftell(f_phindex) / 4;
		intp = before_tab;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}
	if(after_tab[0] != 0)
	{
		ph->after = ftell(f_phindex) / 4;
		intp = after_tab;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}
	if(default_spect[0] != 0)
	{
		ph->spect = ftell(f_phindex) / 4;
		intp = default_spect;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}

	if(ph->phflags & phVOICED)
	{
		if(ph->type == phSTOP)
			ph->type = phVSTOP;
		else
		if(ph->type == phFRICATIVE)
			ph->type = phVFRICATIVE;
	}

	ph->std_length |= 0x8000;  // 'locally declared' indicator

	return(phindex);
}  // end of Compile::CPhoneme



void Compile::WritePhonemeTable()
{//==============================
	int ix;
	int j;
	int n;
	int value;
	int count;
	PHONEME_TAB *p;

	value = n_phoneme_tabs;
	fwrite(&value,4,1,f_phdata);

	for(ix=0; ix<n_phoneme_tabs; ix++)
	{
		p = phoneme_tab_list2[ix].phoneme_tab_ptr;
		n = n_phcodes_list[ix];
		p[n].mnemonic = 0;   // terminate the phoneme table

		// count number of locally declared phonemes
		count=0;
		for(j=0; j<n; j++)
		{
			if(ix==0)
				p[j].std_length |= 0x8000;  // write all phonemes in the base phoneme table

			if(p[j].std_length & 0x8000)
				count++;
		}
		phoneme_tab_list2[ix].n_phonemes = count+1;

		fputc(count+1,f_phdata);
		fputc(phoneme_tab_list2[ix].includes,f_phdata);
		fputc(0,f_phdata);
		fputc(0,f_phdata);
		fwrite(phoneme_tab_list2[ix].name,1,N_PHONEME_TAB_NAME,f_phdata);

		for(j=0; j<n; j++)
		{
			if(p[j].std_length & 0x8000)
			{
				// this bit is set temporarily to incidate a local phoneme, declared in
				// in the current phoneme file
				p[j].std_length &= 0x7fff;
				fwrite(&p[j],sizeof(PHONEME_TAB),1,f_phdata);
			}
		}
		fwrite(&p[n],sizeof(PHONEME_TAB),1,f_phdata);  // include the extra list-terminator phoneme entry
		free(p);
	}
}


void Compile::EndPhonemeTable()
{//============================
	int  ix;

	if(n_phoneme_tabs == 0)
		return;

	// check that all referenced phonemes have been declared
	for(ix=0; ix<n_phcodes; ix++)
	{
		if(phoneme_tab2[ix].type == phINVALID)
		{
			fprintf(f_errors,"%3d: Phoneme [%s] not declared, referenced at line %d\n",linenum,
				WordToString(phoneme_tab2[ix].mnemonic),int(phoneme_tab2[ix].spect));
			error_count++;
		}
	}

	n_phcodes_list[n_phoneme_tabs-1] = n_phcodes;
}


void Compile::StartPhonemeTable(const char *name)
{//==============================================
	int ix;
	int j;
	PHONEME_TAB *p;

	fprintf(f_errors,"______________________________\nPhoneme Table: '%s'\n",name);

	if(n_phoneme_tabs >= N_PHONEME_TABS-1)
	{
		Error("Too many phonemetables");
		return;
	}
	p = (PHONEME_TAB *)calloc(sizeof(PHONEME_TAB),N_PHONEME_TAB);

	if(p == NULL)
	{
		Error("Out of memory");
		return;
	}

	if(gui_flag)
		progress->Update(n_phoneme_tabs);

	phoneme_tab_list2[n_phoneme_tabs].phoneme_tab_ptr = phoneme_tab2 = p;
	memset(phoneme_tab_list2[n_phoneme_tabs].name, 0, N_PHONEME_TAB_NAME);
	strncpy0(phoneme_tab_list2[n_phoneme_tabs].name, name, N_PHONEME_TAB_NAME);
	n_phcodes = 1;
	phoneme_tab_list2[n_phoneme_tabs].includes = 0;

	if(n_phoneme_tabs > 0)
	{
		NextItem(tSTRING);	// name of base phoneme table
		for(ix=0; ix<n_phoneme_tabs; ix++)
		{
			if(strcmp(item_string,phoneme_tab_list2[ix].name)==0)
			{
				phoneme_tab_list2[n_phoneme_tabs].includes = ix+1;

				// initialise the new phoneme table with the contents of this one
				memcpy(phoneme_tab2,phoneme_tab_list2[ix].phoneme_tab_ptr,sizeof(PHONEME_TAB)*N_PHONEME_TAB);
				n_phcodes = n_phcodes_list[ix];

				// clear "local phoneme" bit"
				for(j=0; j<n_phcodes; j++)
					phoneme_tab2[j].std_length = phoneme_tab2[j].std_length & 0x7fff;
				break;
			}
		}
		if(ix == n_phoneme_tabs)
		{
			Error("Can't find base phonemetable '%s'",item_string);
		}
	}

	n_phoneme_tabs++;
}  // end of StartPhonemeTable



void Compile::CPhonemeFiles(char *path_source)
{//===========================================
	int phcode;
	int item;
	FILE *f;
	char buf[120];

	linenum = 1;
	n_phcodes = 1;  // don't use phoneme code=0, it's used as string terminator

	count_references = 0;
	duplicate_references = 0;
	count_frames = 0;

	for(;;)
	{
		if(feof(f_in))
		{
			// end of file, go back to previous from, from which this was included

			if(stack_ix == 0)
				break;   // end of top level, finished
			fclose(f_in);
			f_in = stack[--stack_ix].file;
			linenum = stack[stack_ix].linenum;
			fprintf(f_errors,"\n\n");
		}

		item = NextItem(tKEYWORD);

		if(item == tINCLUDE)
		{
			NextItem(tSTRING);
			sprintf(buf,"%s%s",path_source,item_string);
			if((stack_ix < N_STACK) && (f = fopen_log(f_errors,buf,"r")) != NULL)
			{
				fprintf(f_errors,"include %s\n",item_string);
				stack[stack_ix].linenum = linenum;
				stack[stack_ix++].file = f_in;
				
				f_in = f;
				linenum = 1;
			}
		}
		else
		if(item == tPHONEMETABLE)
		{
			EndPhonemeTable();
			NextItem(tSTRING);	// name of the new phoneme table
			StartPhonemeTable(item_string);
		}
		else
		if(item == tPHONEMENUMBER)
		{
			// reserve a specified number for this phoneme
			phcode = NextItem(tNUMBER);
			NextItem(tSTRING);
			ReservePhCode(item_string,phcode);
		}
		else
		if(item == tPHONEMESTART)
		{
			if(n_phoneme_tabs == 0)
			{
				Error("phonemetable is missing");
				return;
			}
			phcode = CPhoneme();
		}
		else
		if(!feof(f_in))
			Error("Keyword 'phoneme' expected");
	}
	phoneme_tab2[n_phcodes+1].mnemonic = 0;  // terminator
}  //  end of CPhonemeFiles


#define MAKE_ENVELOPES
#ifdef  MAKE_ENVELOPES 

#define ENV_LEN  128
void MakeEnvelope(unsigned char *env, float *points_x, float *points_y)
{//====================================================================
	int ix = -1;
	int x,y;
	
	for(x=0; x<ENV_LEN; x++)
	{
		if(x > points_x[ix+4])
			ix++;

		y = (int)polint(&points_x[ix],&points_y[ix],4,x);
		if(y < 0) y = 0;
		if(y > 255) y = 255;
		env[x] = y;
	}	
}

static float env1_x[]={0,0x30,0x40,ENV_LEN};
static float env1_y[]={0,200,255,255};
static float env2_x[]={0,0x28,0x48,0x60,ENV_LEN};
static float env2_y[]={255,0xe0,0xc0,0x60,0};
static float env3_x[]={0,0x18,0x30,ENV_LEN};
static float env3_y[]={0,0x28,0x50,255};
static float env4_x[]={0,0x20,0x40,0x60,ENV_LEN};
static float env4_y[]={255,0x70,0,0x70,255};
static float env5_x[]={0,0x20,0x40,0x58,0x70,ENV_LEN};
static float env5_y[]={0,0x28,0x50,0xa0,0xf0,255};
static float env6_x[]={0,0x20,0x38,0x60,ENV_LEN};
static float env6_y[]={255,0xe8,0xd0,0x68,0};

static float env7_x[]={0,0x20,0x40,0x60,ENV_LEN};    // 214
static float env7_y[]={85,42,0,128,255};
static float env8_x[]={0,0x20,0x40,0x60,ENV_LEN};    // 211
static float env8_y[]={255,130,20,10,0};
static float env9_x[]={0,0x20,0x40,0x60,ENV_LEN};    // 51 fall
static float env9_y[]={255,210,140,70,0};

static float enva3_x[]={0,44,64,84,ENV_LEN};  // amp env for broken tone
static float enva3_y[]={255,255,160,255,255};
static float enva6_x[]={0,44,64,80,ENV_LEN};  // amp env for drop tone
static float enva6_y[]={255,255,255,250,50};


unsigned char env_test[ENV_LEN];

void MakeEnvFile(char *fname, float *x, float *y, int source)
{//==========================================================
	static char hdr[12] = {'P','I','T','C','H','E','N','V',80,0,120,0};
	FILE *f;
	int ix;

	MakeEnvelope(env_test,x,y);
	f = fopen(fname,"w");

	if(source)
	{
		for(ix=0; ix<128; ix++)
		{
			fprintf(f,"0x%.2x,",env_test[ix]);
			if((ix & 7) == 7)
				fprintf(f,"\n");
		}
	}
	else
	{
		fwrite(hdr,12,1,f);
		fwrite(env_test,128,1,f);
	}
	fclose(f);
	
}


void make_envs()
{//=============
	MakeEnvFile("p_level",env1_x,env1_y,0);
	MakeEnvFile("p_rise",env5_x,env5_y,0);
	MakeEnvFile("p_fall",env9_x,env9_y,0);
	MakeEnvFile("p_214",env7_x,env7_y,0);
	MakeEnvFile("p_211",env8_x,env8_y,0);


	MakeEnvFile("vi_2",env2_x,env2_y,0);
	MakeEnvFile("vi_3",env3_x,env3_y,0);
	MakeEnvFile("p_fallrise",env4_x,env4_y,0);
	MakeEnvFile("vi_6",env6_x,env6_y,0);


	MakeEnvFile("vi_3amp",enva3_x,enva3_y,0);
	MakeEnvFile("vi_6amp",enva6_x,enva6_y,0);

}
#endif

static int ref_sorter(char **a, char **b)
{//======================================
	REF_HASH_TAB *p1 = (REF_HASH_TAB *)(*a);
 	REF_HASH_TAB *p2 = (REF_HASH_TAB *)(*b);
  return(strcmp(p1->string,p2->string));
}   /* end of strcmp2 */



void Compile::Report(void)
{//=======================
	int ix;
	int hash;
	int n;
	REF_HASH_TAB *p;
	REF_HASH_TAB **list;
	FILE *f_report;
	char fname[80];

	// make a list of all the references and sort it
	list = (REF_HASH_TAB **)malloc(count_references * sizeof(REF_HASH_TAB *));
	if(list == NULL)
		return;

	sprintf(fname,"%scompile_report",path_source);
	f_report = fopen(fname,"w");
	if(f_report == NULL)
	{
		free(list);
		return;
	}

	fprintf(f_report,"%d phoneme tables\n",n_phoneme_tabs);
   fprintf(f_report,"          new total\n");
	for(ix=0; ix<n_phoneme_tabs; ix++)
	{
		fprintf(f_report,"%8s %3d %4d\n",phoneme_tab_list2[ix].name, phoneme_tab_list2[ix].n_phonemes, n_phcodes_list[ix]+1);
	}
	fputc('\n',f_report);

	ix = 0;
	for(hash=0; (hash < 256) && (ix < count_references); hash++)
	{
		p = ref_hash_tab[hash];
		while(p != NULL)
		{
			list[ix++] = p;
			p = (REF_HASH_TAB *)(p->link);
		}
	}
	n = ix;
	qsort((void *)list,n,sizeof(REF_HASH_TAB *),(int (*)(const void *,const void *))ref_sorter);
	
	for(ix=0; ix<n; ix++)
	{
		int j, ph_tab_num;

		fprintf(f_report,"%3d  %s",list[ix]->n_uses, list[ix]->string);
		for(j = strlen(list[ix]->string); j < 14; j++)
		{
			fputc(' ',f_report);  // pad filename with spaces
		}
		for(j=0; (j < list[ix]->n_used_by) && (j<N_USED_BY); j++)
		{
			ph_tab_num = list[ix]->used_by[j];
			fprintf(f_report," %s",phoneme_tab_list2[ph_tab_num-1].name);
		}
		if(j < list[ix]->n_used_by)
		{
			fprintf(f_report," ++");
		}
		fputc('\n',f_report);
	}
	free(list);
	fclose(f_report);
}



wxString CompileAllDictionaries()
{//==============================
	wxString filename;
	wxFileName fname;
	wxString dictstr;
	wxString report = _T("");
	int err;
	int errors = 0;
	int dict_count = 0;
	FILE *log;
	char dictname[80];
	char fname_log[80];
	char save_voice_name[80];

	if(!wxDirExists(path_dictsource))
	{
		if(gui_flag)
		{
			wxString dirname = wxDirSelector(_T("Directory of dictionary files"),path_phsource);
			if(!dirname.IsEmpty())
			{
				path_dictsource = dirname;
				strncpy0(path_dsource,path_dictsource.mb_str(wxConvLocal),sizeof(path_dsource)-1);
				strcat(path_dsource,"/");
			}
		}
		else
		{
			fprintf(stderr,"Can't find dictionary files: %s\n",path_dsource);
		}
	}

	wxDir dir(path_dictsource);
	if(!dir.IsOpened())
	{
		return(_T(" No dictionaries"));
	}

	strcpy(save_voice_name,voice_name);

	sprintf(fname_log,"%s%s",path_dsource,"dict_log");
	log = fopen(fname_log,"w");

	bool cont = dir.GetFirst(&filename, _T("*_rules"), wxDIR_FILES);
	while ( cont )
	{
		fname = wxFileName(filename);
		dictstr = fname.GetName().BeforeLast('_');
		strcpy(dictname,dictstr.mb_str(wxConvLocal));
		dict_count++;

		LoadVoice(dictname,0);

		if((err = CompileDictionary(path_dsource, dictname,log,NULL)) > 0)
		{
			report = report + dictstr + wxString::Format(_T(" %d, "),err);
			errors += err;
		}

		cont = dir.GetNext(&filename);
	}
	if(log != NULL)
		fclose(log);

	LoadVoice(save_voice_name,1);

	if(errors == 0)
		return(wxString::Format(_T(" Compiled %d dictionaries"),dict_count));
	else
	{
		return(_T(" Dictionary errors: ") + report);
	}

}  // end of CompileAllDictionaries



void Compile::CPhonemeTab(const char *source)
{//========================================
	int label;
	char fname[120];
	wxString report;
	wxString report_dict;

#ifdef MAKE_ENVELOPES
make_envs();	
#endif

	error_count = 0;
memset(markers_used,0,sizeof(markers_used));

	f_errors = stderr;

	sprintf(fname,"%s%s",path_source,source);
	f_in = fopen_log(f_errors,fname,"r");
	if(f_in == NULL)
	{
		if(gui_flag)
		{
			wxString dir = wxDirSelector(_T("Directory for 'phonemes' file"),path_phsource);
			if(!dir.IsEmpty())
			{
				path_phsource = dir;
				strncpy0(path_source,path_phsource.mb_str(wxConvLocal),sizeof(path_source)-1);
				strcat(path_source,"/");
			}
		}

		sprintf(fname,"%s%s",path_source,source);
		f_in = fopen_log(f_errors,fname,"r");
		if(f_in == NULL)
		{
			wxLogError(_T("Can't read master phonemes file:\n") + wxString(fname,wxConvLocal));
			return;
		}
	}

	progress_max = 0;
	while(fgets(fname,sizeof(fname),f_in) != NULL)
	{
		// count the number of phoneme tables declared in the master phonemes file
		if(memcmp(fname,"phonemetable",12)==0)
			progress_max++;
	}
	rewind(f_in);

	sprintf(fname,"%s/%s",path_home,"phondata");
	f_spects = fopen_log(f_errors,fname,"w");
	sprintf(fname,"%s%s",path_source,"error_log");
	if((f_errors = fopen_log(f_errors,fname,"w")) == NULL)
		f_errors = stderr;
	sprintf(fname,"%s/%s",path_home,"phonindex");
	f_phindex = fopen_log(f_errors,fname,"w");
	sprintf(fname,"%s/%s",path_home,"phontab");
	f_phdata = fopen_log(f_errors,fname,"w");

	if(f_spects==NULL || f_phindex==NULL || f_phdata==NULL)
	{
		return;
	}

	if(gui_flag)
	{
		progress = new wxProgressDialog(_T("Phonemes"),_T(""),progress_max);
	}
	else
	{
		fprintf(stderr,"Compiling phoneme data: %s\n",path_source);
	}

	// write a word so that further data doesn't start at displ=0
	label = 0;
	fwrite(&label,4,1,f_spects);
	fwrite(&label,4,1,f_phindex);

	memset(ref_hash_tab,0,sizeof(ref_hash_tab));

	n_phoneme_tabs = 0;
	stack_ix = 0;
	StartPhonemeTable("base");
	CPhonemeFiles(path_source);

	EndPhonemeTable();
	WritePhonemeTable();

fprintf(f_errors,"Refs %d,  Reused %d\n",count_references,duplicate_references);
	fclose(f_in);
	fclose(f_spects);
	fclose(f_errors);
	fclose(f_phindex);
	fclose(f_phdata);

	LoadPhData();
	LoadVoice(voice_name,0);
	Report();


	report_dict = CompileAllDictionaries();

	if(gui_flag)
	{
		delete progress;
	}

	report.Printf(_T("Compiled phonemes: %d errors."),error_count);
	if(error_count > 0)
	{
		report += _T(" See 'error.log'.");
		wxLogError(report);
	}
	wxLogStatus(report + report_dict);

	if(gui_flag == 0)
	{
		strcpy(fname,(report+report_dict).mb_str(wxConvLocal));
		fprintf(stderr,"%s\n",fname);
		
	}
}  // end of Compile::CPhonemeTab




void CompileInit(void)
{
	compile = new Compile;
	compile->CPhonemeTab("phonemes");
	delete compile;
}
