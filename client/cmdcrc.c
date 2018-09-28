//-----------------------------------------------------------------------------
// Copyright (C) 2015 iceman <iceman at iuse.se>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// CRC Calculations from the software reveng commands
//-----------------------------------------------------------------------------

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#  ifndef STDIN_FILENO
#    define STDIN_FILENO 0
#  endif /* STDIN_FILENO */
#endif /* _WIN32 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cmdmain.h"
#include "cmdcrc.h"
#include "reveng/reveng.h"
#include "ui.h"
#include "util.h"

#define MAX_ARGS 20

int uerr(char *msg){
	PrintAndLog("%s",msg);
	return 0;
}

int split(char *str, char *arr[MAX_ARGS]){
	int beginIndex = 0;
	int endIndex;
	int maxWords = MAX_ARGS;
	int wordCnt = 0;

	while(1){
		while(isspace(str[beginIndex])){
			++beginIndex;
		}
		if(str[beginIndex] == '\0') {
			break;
		}
		endIndex = beginIndex;
		while (str[endIndex] && !isspace(str[endIndex])){
			++endIndex;
		}
		int len = endIndex - beginIndex;
		char *tmp = calloc(len + 1, sizeof(char));
		memcpy(tmp, &str[beginIndex], len);
		arr[wordCnt++] = tmp;
		//PrintAndLog("DEBUG cnt: %d, %s",wordCnt-1, arr[wordCnt-1]);
		beginIndex = endIndex;
		if (wordCnt == maxWords)
			break;
	}
	return wordCnt;
}

int CmdCrc(const char *Cmd)
{
	char name[] = {"reveng "};
	char Cmd2[50 + 7];
	memcpy(Cmd2, name, 7);
	memcpy(Cmd2 + 7, Cmd, 50);
	char *argv[MAX_ARGS];
	int argc = split(Cmd2, argv);

	if (argc == 3 && memcmp(argv[1],"-g",2)==0) {
		CmdrevengSearch(argv[2]);
	} else {
		reveng_main(argc, argv);
	}
	//PrintAndLog("DEBUG argc: %d, %s %s Cmd: %s",argc, argv[0], Cmd2, Cmd);
	for(int i = 0; i < argc; ++i){
		free(argv[i]);
	}

	return 0;
}

//returns array of model names and the count of models returning
//  as well as a width array for the width of each model
int GetModels(char *Models[], int *count, uint8_t *width){
	/* default values */
	static model_t model = {
		PZERO,		/* no CRC polynomial, user must specify */
		PZERO,		/* Init = 0 */
		P_BE,		/* RefIn = false, RefOut = false, plus P_RTJUST setting in reveng.h */
		PZERO,		/* XorOut = 0 */
		PZERO,		/* check value unused */
		NULL		/* no model name */
	};

	int ibperhx = 8;//, obperhx = 8;
	int rflags = 0, uflags = 0; /* search and UI flags */
	poly_t apoly, crc, qpoly = PZERO, *apolys = NULL, *pptr = NULL, *qptr = NULL;
	model_t pset = model, *candmods, *mptr;

	/* stdin must be binary */
	#ifdef _WIN32
		_setmode(STDIN_FILENO, _O_BINARY);
	#endif /* _WIN32 */

	SETBMP();
	
	int args = 0, psets, pass;
	int Cnt = 0;
	if (width[0] == 0) { //reveng -D
		*count = mcount();
		if(!*count)
			return uerr("no preset models available");

		for(int mode = 0; mode < *count; ++mode) {
			mbynum(&model, mode);
			mcanon(&model);
			size_t size = (model.name && *model.name) ? strlen(model.name) : 6;
			char *tmp = calloc(size+1, sizeof(char));
			if (tmp==NULL)
				return uerr("out of memory?");

			memcpy(tmp, model.name, size);
			Models[mode] = tmp;
			width[mode] = plen(model.spoly);
		}
		mfree(&model);
	} else { //reveng -s

		if(~model.flags & P_MULXN)
			return uerr("cannot search for non-Williams compliant models");

		praloc(&model.spoly, (unsigned long)width[0]);
		praloc(&model.init, (unsigned long)width[0]);
		praloc(&model.xorout, (unsigned long)width[0]);
		if(!plen(model.spoly))
			palloc(&model.spoly, (unsigned long)width[0]);
		else
			width[0] = (uint8_t)plen(model.spoly);

		/* special case if qpoly is zero, search to end of range */
		if(!ptst(qpoly))
			rflags &= ~R_HAVEQ;


		/* not going to be sending additional args at this time (maybe future?)

		// allocate argument array 
		args = argc - optind;
		if(!(apolys = malloc(args * sizeof(poly_t))))
			return uerr("cannot allocate memory for argument list");

		for(pptr = apolys; optind < argc; ++optind) {
			if(uflags & C_INFILE)
				*pptr++ = rdpoly(argv[optind], model.flags, ibperhx);
			else
				*pptr++ = strtop(argv[optind], model.flags, ibperhx);
		}
		// exit value of pptr is used hereafter! 
	
		*/

		/* if endianness not specified, try
		 * little-endian then big-endian.
		 * NB: crossed-endian algorithms will not be
		 * searched.
		 */
		/* scan against preset models */
		if(~uflags & C_FORCE) {
			pass = 0;
			Cnt = 0;
			do {
				psets = mcount();
				//PrintAndLog("psets: %d",psets);
				while(psets) {
					mbynum(&pset, --psets);
					
					/* skip if different width, or refin or refout don't match */
					if(plen(pset.spoly) != width[0] || (model.flags ^ pset.flags) & (P_REFIN | P_REFOUT))
						continue;
					/* skip if the preset doesn't match specified parameters */
					if(rflags & R_HAVEP && pcmp(&model.spoly, &pset.spoly))
						continue;
					if(rflags & R_HAVEI && psncmp(&model.init, &pset.init))
						continue;
					if(rflags & R_HAVEX && psncmp(&model.xorout, &pset.xorout))
						continue;
			
					//for additional args (not used yet, maybe future?)
					apoly = pclone(pset.xorout);
					if(pset.flags & P_REFOUT)
						prev(&apoly);
					
					for(qptr = apolys; qptr < pptr; ++qptr) {
						crc = pcrc(*qptr, pset.spoly, pset.init, apoly, 0);
						if(ptst(crc)) {
							pfree(&crc);
							break;
						} else
							pfree(&crc);
					}
					pfree(&apoly);
					if(qptr == pptr) {

						/* the selected model solved all arguments */

						mcanon(&pset);
						
						size_t size = (pset.name && *pset.name) ? strlen(pset.name) : 6;
						//PrintAndLog("Size: %d, %s, count: %d",size,pset.name, Cnt);
						char *tmp = calloc(size+1, sizeof(char));
						if (tmp==NULL){
							PrintAndLog("out of memory?");
							return 0;
						}
						width[Cnt] = width[0];
						memcpy(tmp, pset.name, size);
						Models[Cnt++] = tmp;
						*count = Cnt;
						uflags |= C_RESULT;
					}
				}
				mfree(&pset);

				/* toggle refIn/refOut and reflect arguments */
				if(~rflags & R_HAVERI) {
					model.flags ^= P_REFIN | P_REFOUT;
					for(qptr = apolys; qptr < pptr; ++qptr)
						prevch(qptr, ibperhx);
				}
			} while(~rflags & R_HAVERI && ++pass < 2);
		}
		//got everything now free the memory...

		if(uflags & C_RESULT) {
			for(qptr = apolys; qptr < pptr; ++qptr)
				pfree(qptr);
		}
		if(!(model.flags & P_REFIN) != !(model.flags & P_REFOUT))
			return uerr("cannot search for crossed-endian models");

		pass = 0;
		do {
			mptr = candmods = reveng(&model, qpoly, rflags, args, apolys);
			if(mptr && plen(mptr->spoly))
				uflags |= C_RESULT;
			while(mptr && plen(mptr->spoly)) {
				mfree(mptr++);
			}
			free(candmods);
			if(~rflags & R_HAVERI) {
				model.flags ^= P_REFIN | P_REFOUT;
				for(qptr = apolys; qptr < pptr; ++qptr)
					prevch(qptr, ibperhx);
			}
		} while(~rflags & R_HAVERI && ++pass < 2);
		for(qptr = apolys; qptr < pptr; ++qptr)
			pfree(qptr);
		free(apolys);
		if(~uflags & C_RESULT)
			return uerr("no models found");
		mfree(&model);
	}
	return 1;
}

//test call to GetModels
int CmdrevengTest(const char *Cmd){
	char *Models[80];
	int count = 0;
	uint8_t widtharr[80] = {0};
	uint8_t width = 0;
	width = param_get8(Cmd, 0);
	//PrintAndLog("width: %d",width);
	if (width > 89)
		return uerr("Width cannot exceed 89");

	widtharr[0] = width;
	int ans = GetModels(Models, &count, widtharr);
	if (!ans) return 0;
	
	PrintAndLog("Count: %d",count);
	for (int i = 0; i < count; i++){
		PrintAndLog("Model %d: %s, width: %d",i,Models[i], widtharr[i]);
		free(Models[i]);
	}
	return 1;
}

//-c || -v
//inModel = valid model name string - CRC-8
//inHexStr = input hex string to calculate crc on
//reverse = reverse calc option if true
//endian = {0 = calc default endian input and output, b = big endian input and output, B = big endian output, r = right justified
//          l = little endian input and output, L = little endian output only, t = left justified}
//result = calculated crc hex string
int RunModel(char *inModel, char *inHexStr, bool reverse, char endian, char *result){
	/* default values */
	static model_t model = {
		PZERO,		// no CRC polynomial, user must specify
		PZERO,		// Init = 0
		P_BE,		  // RefIn = false, RefOut = false, plus P_RTJUST setting in reveng.h
		PZERO,		// XorOut = 0
		PZERO,		// check value unused 
		NULL		  // no model name 
	};
	int ibperhx = 8, obperhx = 8;
	int rflags = 0; // search flags 
	int c;
	//unsigned long width;
	poly_t apoly, crc;

	char *string;

	// stdin must be binary
	#ifdef _WIN32
		_setmode(STDIN_FILENO, _O_BINARY);
	#endif /* _WIN32 */

	SETBMP();
	//set model
	if(!(c = mbynam(&model, inModel))) {
		fprintf(stderr,"error: preset model '%s' not found.  Use reveng -D to list presets.\n", inModel);
		return 0;
	}
	if(c < 0)
		return uerr("no preset models available");

	// must set width so that parameter to -ipx is not zeroed 
	//width = plen(model.spoly);
	rflags |= R_HAVEP | R_HAVEI | R_HAVERI | R_HAVERO | R_HAVEX;
	
	//set flags
	switch (endian) {
		case 'b': /* b  big-endian (RefIn = false, RefOut = false ) */
			model.flags &= ~P_REFIN;
			rflags |= R_HAVERI;
			/* fall through: */
		case 'B': /* B  big-endian output (RefOut = false) */
			model.flags &= ~P_REFOUT;
			rflags |= R_HAVERO;
			mnovel(&model);
			/* fall through: */
		case 'r': /* r  right-justified */
			model.flags |= P_RTJUST;
			break;
		case 'l': /* l  little-endian input and output */
			model.flags |= P_REFIN;
			rflags |= R_HAVERI;
			/* fall through: */
		case 'L': /* L  little-endian output */
			model.flags |= P_REFOUT;
			rflags |= R_HAVERO;
			mnovel(&model);
			/* fall through: */
		case 't': /* t  left-justified */
			model.flags &= ~P_RTJUST;
			break;
	}

	mcanon(&model);

	if (reverse) {
		// v  calculate reversed CRC
		/* Distinct from the -V switch as this causes
		 * the arguments and output to be reversed as well.
		 */
		// reciprocate Poly
		prcp(&model.spoly);

		/* mrev() does:
		 *   if(refout) prev(init); else prev(xorout);
		 * but here the entire argument polynomial is
		 * reflected, not just the characters, so RefIn
		 * and RefOut are not inverted as with -V.
		 * Consequently Init is the mirror image of the
		 * one resulting from -V, and so we have:
		 */
		if(~model.flags & P_REFOUT) {
			prev(&model.init);
			prev(&model.xorout);
		}

		// swap init and xorout
		apoly = model.init;
		model.init = model.xorout;
		model.xorout = apoly;
	}
	// c  calculate CRC

	// validate inputs
	/* if(plen(model.spoly) == 0) {
	 *	fprintf(stderr,"%s: no polynomial specified for -%c (add -w WIDTH -p POLY)\n", myname, mode);
	 *	exit(EXIT_FAILURE);
	 * }
	 */

	/* in the Williams model, xorout is applied after the refout stage.
	 * as refout is part of ptostr(), we reverse xorout here.
	 */
	if(model.flags & P_REFOUT)
		prev(&model.xorout);

	apoly = strtop(inHexStr, model.flags, ibperhx);

	if(reverse)
		prev(&apoly);

	crc = pcrc(apoly, model.spoly, model.init, model.xorout, model.flags);

	if(reverse)
		prev(&crc);

	string = ptostr(crc, model.flags, obperhx);
	for (int i = 0; i < 50; i++){
		result[i] = string[i];
		if (result[i]==0) break;
	}
	free(string);
	pfree(&crc);
	pfree(&apoly);
	return 1;
}

//test call to RunModel
int CmdrevengTestC(const char *Cmd){
	int cmdp = 0;
	char inModel[30] = {0x00};
	char inHexStr[30] = {0x00};
	char result[30];
	int dataLen;
	char endian = 0;
	dataLen = param_getstr(Cmd, cmdp++, inModel);
	if (dataLen < 4) return 0;
	dataLen = param_getstr(Cmd, cmdp++, inHexStr);
	if (dataLen < 4) return 0;
	bool reverse = (param_get8(Cmd, cmdp++)) ? true : false;
	endian = param_getchar(Cmd, cmdp++); 

	//PrintAndLog("mod: %s, hex: %s, rev %d", inModel, inHexStr, reverse);
	int ans = RunModel(inModel, inHexStr, reverse, endian, result);
	if (!ans) return 0;
	
	PrintAndLog("Result: %s",result);
	return 1;
}

//returns a calloced string (needs to be freed)
char *SwapEndianStr(const char *inStr, const size_t len, const uint8_t blockSize){
	char *tmp = calloc(len+1, sizeof(char));
	for (uint8_t block=0; block < (uint8_t)(len/blockSize); block++){
		for (size_t i = 0; i < blockSize; i+=2){
			tmp[i+(blockSize*block)] = inStr[(blockSize-1-i-1)+(blockSize*block)];
			tmp[i+(blockSize*block)+1] = inStr[(blockSize-1-i)+(blockSize*block)];
		}
	}
	return tmp;
}

// takes hex string in and searches for a matching result (hex string must include checksum)
int CmdrevengSearch(const char *Cmd){
	char inHexStr[50] = {0x00};
	int dataLen = param_getstr(Cmd, 0, inHexStr);
	if (dataLen < 4) return 0;

	char *Models[80];
	int count = 0;
	uint8_t width[80];
	width[0] = 0;
	uint8_t crcChars = 0;
	char result[30];
	char revResult[30];
	int ans = GetModels(Models, &count, width);
	bool found = false;
	if (!ans) return 0;
	
	// try each model and get result
	for (int i = 0; i < count; i++){
		/*if (found) {
			free(Models[i]);
			continue;
		}*/
		// round up to # of characters in this model's crc
		crcChars = ((width[i]+7)/8)*2; 
		// can't test a model that has more crc digits than our data
		if (crcChars >= dataLen) 
			continue;
		memset(result, 0, 30);
		char *inCRC = calloc(crcChars+1, sizeof(char));
		memcpy(inCRC, inHexStr+(dataLen-crcChars), crcChars);

		char *outHex = calloc(dataLen-crcChars+1, sizeof(char));
		memcpy(outHex, inHexStr, dataLen-crcChars);

		//PrintAndLog("DEBUG: dataLen: %d, crcChars: %d, Model: %s, CRC: %s, width: %d, outHex: %s",dataLen, crcChars, Models[i], inCRC, width[i], outHex);
		ans = RunModel(Models[i], outHex, false, 0, result);
		if (ans) {
			//test for match
			if (memcmp(result, inCRC, crcChars)==0){
				PrintAndLog("\nFound a possible match!\nModel: %s\nValue: %s\n",Models[i], result);
				//optional - stop searching if found...
				found = true;
			} else {
				if (crcChars > 2){
					char *swapEndian = SwapEndianStr(result, crcChars, crcChars);
					if (memcmp(swapEndian, inCRC, crcChars)==0){
						PrintAndLog("\nFound a possible match!\nModel: %s\nValue EndianSwapped: %s\n",Models[i], swapEndian);
						//optional - stop searching if found...
						found = true;
					}
					free(swapEndian);
				}
			}
		}
		
		//if (!found){
			ans = RunModel(Models[i], outHex, true, 0, revResult);
			if (ans) {
				//test for match
				if (memcmp(revResult, inCRC, crcChars)==0){
					PrintAndLog("\nFound a possible match!\nModel Reversed: %s\nValue: %s\n",Models[i], revResult);
					//optional - stop searching if found...
					found = true;
				} else {
					if (crcChars > 2){
						char *swapEndian = SwapEndianStr(revResult, crcChars, crcChars);
						if (memcmp(swapEndian, inCRC, crcChars)==0){
							PrintAndLog("\nFound a possible match!\nModel Reversed: %s\nValue EndianSwapped: %s\n",Models[i], swapEndian);
							//optional - stop searching if found...
							found = true;
						}
						free(swapEndian);
					}
				}
			}
		//}
		free(inCRC);
		free(outHex);
		free(Models[i]);
	}
	if (!found) PrintAndLog("\nNo matches found\n");
	return 1;
}
