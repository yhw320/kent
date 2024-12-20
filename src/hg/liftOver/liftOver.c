/* liftOver - Move annotations from one assembly to another. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chain.h"
#include "bed.h"
#include "genePred.h"
#include "sample.h"
#include "liftOver.h"


int bedPlus = 0;
int ends = 0;
bool fudgeThick = FALSE;
bool errorHelp = FALSE;
bool multiple = FALSE;
bool noSerial = FALSE;
bool hasBin = FALSE;
bool tabSep = FALSE;
char *chainTable = NULL;

static struct optionSpec optionSpecs[] = {
    {"bedPlus", OPTION_INT},
    {"chainTable", OPTION_STRING},
    {"errorHelp", OPTION_BOOLEAN},
    {"fudgeThick", OPTION_BOOLEAN},
    {"genePred", OPTION_BOOLEAN},
    {"gff", OPTION_BOOLEAN},
    {"hasBin", OPTION_BOOLEAN},
    {"minBlocks", OPTION_FLOAT},
    {"minChainQ", OPTION_INT},
    {"minChainT", OPTION_INT},
    {"minMatch", OPTION_FLOAT},
    {"minSizeQ", OPTION_INT},
    {"minSizeT", OPTION_INT},
    {"multiple", OPTION_BOOLEAN},
    {"noSerial", OPTION_BOOLEAN},
    {"positions", OPTION_BOOLEAN},
    {"pslT", OPTION_BOOLEAN},
    {"sample", OPTION_BOOLEAN},
    {"ends", OPTION_INT},
    {"tab", OPTION_BOOLEAN},
    {"tabSep", OPTION_BOOLEAN},
    {"preserveInput", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftOver - Move annotations from one assembly to another\n"
  "usage:\n"
  "   liftOver oldFile map.chain newFile unMapped\n"
  "oldFile and newFile are in bed format by default, but can be in GFF and\n"
  "maybe eventually others with the appropriate flags below.\n"
  "The map.chain file has the old genome as the target and the new genome\n"
  "as the query.\n\n"
  "***********************************************************************\n"
  "WARNING: liftOver was only designed to work between different\n"
  "         assemblies of the same organism. It may not do what you want\n"
  "         if you are lifting between different organisms. If there has\n"
  "         been a rearrangement in one of the species, the size of the\n"
  "         region being mapped may change dramatically after mapping.\n"
  "***********************************************************************\n\n"
  "options:\n"
  "   -minMatch=0.N Minimum ratio of bases that must remap. Default %3.2f\n"
  "   -gff  File is in gff/gtf format.  Note that the gff lines are converted\n"
  "         separately.  It would be good to have a separate check after this\n"
  "         that the lines that make up a gene model still make a plausible gene\n"
  "         after liftOver\n"
  "   -genePred - File is in genePred format\n"
  "   -sample - File is in sample format\n"
  "   -bedPlus=N - File is bed N+ format (i.e. first N fields conform to bed format)\n"
  "   -positions - File is in browser \"position\" format\n"
  "   -hasBin - File has bin value (used only with -bedPlus)\n"
  "   -tab - Separate by tabs rather than space (used only with -bedPlus)\n"
  "   -pslT - File is in psl format, map target side only\n"
  "   -ends=N - Lift the first and last N bases of each record and combine the\n"
  "             result. This is useful for lifting large regions like BAC end pairs.\n" 
  "   -minBlocks=0.N Minimum ratio of alignment blocks or exons that must map\n"
  "                  (default %3.2f)\n"
  "   -fudgeThick    (bed 12 or 12+ only) If thickStart/thickEnd is not mapped,\n"
  "                  use the closest mapped base.  Recommended if using \n"
  "                  -minBlocks.\n"
  "   -multiple               Allow multiple output regions\n"
  "   -noSerial               In -multiple mode, do not put a serial number in the 5th BED column\n"
  "   -minChainT, -minChainQ  Minimum chain size in target/query, when mapping\n" 
  "                           to multiple output regions (default 0, 0)\n"
  "   -minSizeT               deprecated synonym for -minChainT (ENCODE compat.)\n"
  "   -minSizeQ               Min matching region size in query with -multiple.\n"
  "   -chainTable             Used with -multiple, format is db.tablename,\n"
  "                               to extend chains from net (preserves dups)\n"
  "   -errorHelp              Explain error messages\n"
  "   -preserveInput          Attach positions from the input file to item names, to assist in\n"
  "                           determining what got mapped where (bed4+, gff, genePred, sample only)\n",
    LIFTOVER_MINMATCH, LIFTOVER_MINBLOCKS
  );
}

void liftOver(char *oldFile, char *mapFile, double minMatch, 
                double minBlocks, int minSizeT, int minSizeQ,
                int minChainT, int minChainQ, bool multiple, bool noSerial, char *chainTable,
                char *newFile, char *unmappedFile, bool preserveInput)
/* liftOver - Move annotations from one assembly to another. */
{
struct hash *chainHash = newHash(0);		/* Old chromosome name keyed, chromMap valued. */
FILE *mapped = mustOpen(newFile, "w");
FILE *unmapped = mustOpen(unmappedFile, "w");
int errCt;

if (!fileExists(oldFile))
    errAbort("Can't find file: %s\n", oldFile);
verbose(1, "Reading liftover chains\n");
readLiftOverMap(mapFile, chainHash);
verbose(1, "Mapping coordinates\n");
if (optionExists("gff"))
    {
    fprintf(stderr, "WARNING: -gff is not recommended.\nUse 'ldHgGene -out=<file.gp>' and then 'liftOver -genePred <file.gp>'\n");
    if (multiple)
        errAbort("ERROR: -multiple is not supported for -gff.");
    if (chainTable)
        errAbort("ERROR: -chainTable is not supported for -gff.");
    liftOverGff(oldFile, chainHash, minMatch, minBlocks, mapped, unmapped, preserveInput);
    }
else if (optionExists("genePred"))
    {
    if (chainTable)
        errAbort("ERROR: -chainTable is not supported for -genePred.");
    liftOverGenePred(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                     mapped, unmapped, multiple, preserveInput);
    }
else if (optionExists("sample"))
    {
    if (multiple)
        errAbort("ERROR: -multiple is not supported for -sample.");
    if (chainTable)
        errAbort("ERROR: -chainTable is not supported for -sample.");
    liftOverSample(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                        mapped, unmapped, preserveInput);
    }
else if (optionExists("pslT"))
    {
    verbose(1, "Consider using pslMap instead of liftOver for PSL.\n");
    if (multiple)
        errAbort("ERROR: -multiple is not supported for -pslT.");
    if (chainTable)
        errAbort("ERROR: -chainTable is not supported for -pslT.");
    liftOverPsl(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                        mapped, unmapped);
    }
else if (optionExists("ends"))
    liftOverBedPlusEnds(oldFile, chainHash, minMatch, minBlocks, 
                minSizeT, minSizeQ, 
                minChainT, minChainQ, fudgeThick, mapped, unmapped, multiple, noSerial,
		chainTable, bedPlus, hasBin, tabSep, ends, &errCt, preserveInput);
else if (optionExists("bedPlus"))
    liftOverBedPlus(oldFile, chainHash, minMatch, minBlocks, 
                minSizeT, minSizeQ, 
                minChainT, minChainQ, fudgeThick, mapped, unmapped, multiple, noSerial,
		chainTable, bedPlus, hasBin, tabSep, &errCt, preserveInput);
else if (optionExists("positions"))
    liftOverPositions(oldFile, chainHash, minMatch, minBlocks, minSizeT, minSizeQ, 
                minChainT, minChainQ, fudgeThick, mapped, unmapped, multiple, 
		chainTable, &errCt);
else
    liftOverBed(oldFile, chainHash, minMatch, minBlocks, minSizeT, minSizeQ, 
                minChainT, minChainQ, fudgeThick, mapped, unmapped, multiple, noSerial,
		chainTable, &errCt, preserveInput);
if (!optionExists("positions"))
/* I guess liftOverPositions closes these files.  This is a little akward though. */
    {
    carefulClose(&mapped);
    carefulClose(&unmapped);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int minSizeT = 0;
int minSizeQ = 0;
int minChainT = 0;
int minChainQ = 0;
double minMatch = LIFTOVER_MINMATCH;
double minBlocks = LIFTOVER_MINBLOCKS;
bool preserveInput = FALSE;

optionInit(&argc, argv, optionSpecs);
minMatch = optionDouble("minMatch", minMatch);
minBlocks = optionDouble("minBlocks", minBlocks);
fudgeThick = optionExists("fudgeThick");
multiple = optionExists("multiple");
noSerial = optionExists("noSerial");
if ((!multiple) && (optionExists("minSizeT")  || optionExists("minSizeQ") ||
		    optionExists("minChainT") || optionExists("minChainQ") ||
		    optionExists("chainTable") || optionExists("noSerial")))
    errAbort("minSizeT/Q, minChainT/Q, noSerial and chainTable can only be used with -multiple.");
if (optionExists("minSizeT") && optionExists("minChainT"))
    errAbort("minSizeT is currently a deprecated synonym for minChainT. Can't set both.");
minSizeT = optionInt("minSizeT", minChainT); /* note: we're setting minChainT */
minSizeQ = optionInt("minSizeQ", minSizeQ);
minChainT = optionInt("minChainT", minChainT);
minChainQ = optionInt("minChainQ", minChainQ);
if (optionExists("bedPlus"))
    {
    bedPlus = optionInt("bedPlus", bedPlus);
    if (bedPlus < 3 || bedPlus > 15)
        errAbort("-bedPlus=%d is out of range -- BED has between 3 and 15 fields.", bedPlus);
    }
ends = optionInt("ends", ends);
hasBin = optionExists("hasBin");
tabSep = optionExists("tab") || optionExists("tabSep");
if ((hasBin || tabSep) && !bedPlus)
    usage();
chainTable = optionVal("chainTable", chainTable);
if (optionExists("errorHelp"))
    errAbort("%s", liftOverErrHelp());
if (optionExists("preserveInput"))
    preserveInput = TRUE;
if (argc != 5)
    usage();
liftOver(argv[1], argv[2], minMatch, minBlocks, minSizeT, minSizeQ, 
	 minChainT, minChainQ, multiple, noSerial, chainTable, argv[3], argv[4], preserveInput);
return 0;
}
