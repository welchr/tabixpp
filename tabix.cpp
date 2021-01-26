#include "tabix.hpp"

using namespace std;

Tabix::Tabix(void) { }

Tabix::Tabix(string& file) {
    has_jumped = false;
    filename = file;
    str.l = 0;
    str.m = 0;
    str.s = NULL;
    const char* cfilename = file.c_str();
    struct stat stat_tbi,stat_vcf;
    char *fnidx = (char*) calloc(strlen(cfilename) + 5, 1);
    strcat(strcpy(fnidx, cfilename), ".tbi");
    if ( bgzf_is_bgzf(cfilename)!=1 )
    {
        free(fnidx);
        throw runtime_error("[tabix++] error reading file " + file + " - was bgzip used to compress this file? And is the file accessible?");
    }
    // Common source of errors: new VCF is used with an old index
    stat(fnidx, &stat_tbi);
    stat(cfilename, &stat_vcf);
    if ( stat_vcf.st_mtime > stat_tbi.st_mtime )
    {
        free(fnidx);
        throw runtime_error("[tabix++] index file " + string(fnidx) + "is older than the vcf/bcf file. Please reindex.");
    }
    free(fnidx);

    if ((fn = hts_open(cfilename, "r")) == 0) {
        throw runtime_error("[tabix++] failed to open data file " + file);
    }

    if ((tbx = tbx_index_load(cfilename)) == NULL) {
        throw runtime_error("[tabix++] failed to load index file for file " + file);
    }

    int nseq;
    const char** seq = tbx_seqnames(tbx, &nseq);
    for (int i=0; i<nseq; i++) {
        chroms.push_back(seq[i]);
    }
    free(seq);

    idxconf = &tbx_conf_vcf;

    // set up the iterator, defaults to the beginning
    current_chrom = chroms.begin();
    iter = tbx_itr_querys(tbx, (current_chrom != chroms.end() ? current_chrom->c_str() : ""));

}

Tabix::~Tabix(void) {
    tbx_itr_destroy(iter);
    tbx_destroy(tbx);
    hts_close(fn);
    free(str.s);
}

const kstring_t * Tabix::getKstringPtr(){
    return &str;
}

void Tabix::getHeader(string& header) {
    header.clear();
    while ( hts_getline(fn, KS_SEP_LINE, &str) >= 0 ) {
        if ( !str.l || str.s[0]!=tbx->conf.meta_char ) {
            break;
        } else {
            header += string(str.s);
            header += "\n";
        }
    }
    // set back to start
    current_chrom = chroms.begin();
    if (iter) tbx_itr_destroy(iter);
    iter = tbx_itr_querys(tbx, (current_chrom != chroms.end() ? current_chrom->c_str() : ""));
}

bool Tabix::setRegion(string& region) {
    tbx_itr_destroy(iter);
    iter = tbx_itr_querys(tbx, region.c_str());
    has_jumped = true;
    return true;
}

bool Tabix::getNextLine(string& line) {
    if (has_jumped) {
        if (iter && tbx_itr_next(fn, tbx, iter, &str) >= 0) {
            line = string(str.s);
            return true;
        } else return false;
    } else { // step through all sequences in the file
        // we've never jumped, so read everything
        if (iter && tbx_itr_next(fn, tbx, iter, &str) >= 0) {
            line = string(str.s);
            return true;
        } else {
            // While we aren't at the end, advance. While we're still not at the end...
            while (current_chrom != chroms.end() && ++current_chrom != chroms.end()) {
                tbx_itr_destroy(iter);
                iter = tbx_itr_querys(tbx, current_chrom->c_str());
                if (iter && tbx_itr_next(fn, tbx, iter, &str) >= 0) {
                    line = string(str.s);
                    return true;
                } else {
                    ++current_chrom;
                }
            }
            return false;
        }
    }
}

bool Tabix::getNextLineKS() {
    if (has_jumped) {
        if (iter && 
	    tbx_itr_next(fn, tbx, iter, &str) >= 0) {
            //line = &str;
            return true;
        } else 
	    return false;
    } else { // step through all sequences in the file
        // we've never jumped, so read everything
        if (iter && tbx_itr_next(fn, tbx, iter, &str) >= 0) {
            //line = &str;
            return true;
        } else {
            // While we aren't at the end, advance. While we're still not at the end...
            while (current_chrom != chroms.end() && ++current_chrom != chroms.end()) {
                tbx_itr_destroy(iter);
                iter = tbx_itr_querys(tbx, current_chrom->c_str());
                if (iter && tbx_itr_next(fn, tbx, iter, &str) >= 0) {
                    //line = &str;
                    return true;
                } else {
                    ++current_chrom;
                }
            }
            return false;
        }
    }
}
