#include "bpp.h"

void indexx(unsigned int n, double arr[], unsigned int indx[]);


/* All of the following have an _st to indicate static */
static long long numpts_st[MAXPATCHFILES], padpts_st[MAXPATCHFILES], N_st;
static int numblks_st[MAXPATCHFILES];
static int bytesperpt_st, bytesperblk_st;
static int numchan_st, numifs_st, ptsperblk_st=PTSPERBLOCK;
static double times_st[MAXPATCHFILES], mjds_st[MAXPATCHFILES];
static double elapsed_st[MAXPATCHFILES], T_st, dt_st;
static double startblk_st[MAXPATCHFILES], endblk_st[MAXPATCHFILES];
static infodata idata_st[MAXPATCHFILES];
static unsigned char chanmask[MAXNUMCHAN], padval;
static unsigned char databuffer[2*MAXDATLEN];
static int currentfile, currentblock;
static int bufferpts=0, padnum=0, shiftbuffer=1;
static double chan_freqs[2*MAXNUMCHAN], mid_freq_st, ch1_freq_st, delta_freq_st;
static int chan_index[2*MAXNUMCHAN];
static int dfb_chan_lookup[MAXREGS][NIBPERREG] = {
  {4, 0, 4, 0},
  {5, 1, 5, 1},
  {6, 2, 6, 2},
  {7, 3, 7, 3},
  {4, 0, 4, 0},
  {5, 1, 5, 1},
  {6, 2, 6, 2},
  {7, 3, 7, 3}
};
/* This takes care of byte swap in outreg_b */
static float sideband_lookup[MAXREGS][NIBPERREG] = {
  {-1.0, -1.0, +1.0, +1.0},
  {-1.0, -1.0, +1.0, +1.0},
  {-1.0, -1.0, +1.0, +1.0},
  {-1.0, -1.0, +1.0, +1.0},
  {+1.0, +1.0, -1.0, -1.0},
  {+1.0, +1.0, -1.0, -1.0},
  {+1.0, +1.0, -1.0, -1.0},
  {+1.0, +1.0, -1.0, -1.0}
};

typedef struct findex{
  double freq; 
  int index;
} findex;

double slaCldj(int iy, int im, int id, int *j);
void convert_BPP_one_IF(unsigned char *rawdata, unsigned char *bytes,
			BPP_ifs ifs);
void convert_BPP_sum_IFs(unsigned char *rawdata, unsigned char *bytes);
void convert_BPP_point(unsigned char *rawdata, unsigned char *bytes);


int compare_findex(const void *ca, const void *cb)
/* qsort comparison function for findex */
{
  findex *a, *b;
 
  a = (findex *) ca;
  b = (findex *) cb;
  if ((b->freq - a->freq) < 0.0)
    return 1;
  if ((b->freq - a->freq) > 0.0)
    return -1;
  /* This keeps IF0 freqs before IF1 freqs*/
  /*
  if ((b->index - a->index) < 0)
    return 1;
  if ((b->index - a->index) > 0)
    return -1;
  */
  return 0;
}


static double UT_strings_to_MJD(char *date, char *start_time, 
				int *mjd_day, double *mjd_fracday)
{
  int mtab[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int julday, year, month=1, day, hour, min, sec, err;

  sscanf(date, "%3d:%4d", &julday, &year);
  julday++;  /* UT date starts from 0, I believe */
  /* Allow for leap years */
  if (year % 4 == 0)
    mtab[1] = 29;
  if (year % 100 == 0 && year % 400 != 0)
    mtab[1] = 28;
  /* Convert Julian day to day and month */
  while (julday - mtab[month-1] > 0){
    julday -= mtab[month-1];
    month++;
  }
  day = julday;
  sscanf(start_time, "%2d:%2d:%2d", &hour, &min, &sec);
  *mjd_fracday = (hour + (min + (sec / 60.0)) / 60.0) / 24.0;
  *mjd_day = slaCldj(year, month, day, &err);
  return *mjd_day + *mjd_fracday;
}


static void swapendian_BPP_header(BPP_SEARCH_HEADER *hdr)
/* This is required since it is a binary header */
{
  int ii;

  hdr->header_version = swap_int(hdr->header_version);
  hdr->bit_mode = swap_int(hdr->bit_mode);
  hdr->num_chans = swap_int(hdr->num_chans);
  hdr->lmst = swap_int(hdr->lmst);
  hdr->scan_file_number = swap_int(hdr->scan_file_number);
  hdr->file_size = swap_int(hdr->file_size);
  hdr->tape_num = swap_int(hdr->tape_num);
  hdr->tape_file_number = swap_int(hdr->tape_file_number);
  hdr->enabled_CBs = swap_int(hdr->enabled_CBs);
  hdr->mb_start_address = swap_int(hdr->mb_start_address);
  hdr->mb_end_address = swap_int(hdr->mb_end_address);
  hdr->mb_start_board = swap_int(hdr->mb_start_board);
  hdr->mb_end_board = swap_int(hdr->mb_end_board);
  hdr->mb_vme_mid_address = swap_int(hdr->mb_vme_mid_address);
  hdr->mb_ack_enabled = swap_int(hdr->mb_ack_enabled);
  hdr->start_from_ste = swap_int(hdr->start_from_ste);
  hdr->cb_sum_polarizations = swap_int(hdr->cb_sum_polarizations);
  hdr->cb_direct_mode = swap_int(hdr->cb_direct_mode);
  hdr->cb_accum_length = swap_int(hdr->cb_accum_length);
  hdr->tb_outs_reg = swap_int(hdr->tb_outs_reg);
  hdr->tb_ste = swap_int(hdr->tb_ste);
  hdr->tb_stc = swap_int(hdr->tb_stc);
  hdr->H_deci_factor = swap_int(hdr->H_deci_factor);
  hdr->GenStat0 = swap_int(hdr->GenStat0);
  hdr->GenStat1 = swap_int(hdr->GenStat1);
  hdr->Ack_Reg = swap_int(hdr->Ack_Reg);
  hdr->dfb_sram_length = swap_int(hdr->dfb_sram_length);
  hdr->ASYMMETRIC = swap_int(hdr->ASYMMETRIC);
  hdr->mb_long_ds0 = swap_int(hdr->mb_long_ds0);
  hdr->aib_serial = swap_int(hdr->aib_serial);
  hdr->aib_rev = swap_int(hdr->aib_rev);
  hdr->BACKEND_TYPE = swap_int(hdr->BACKEND_TYPE);
  hdr->UPDATE_DONE = swap_int(hdr->UPDATE_DONE);
  hdr->HEADER_TYPE = swap_int(hdr->HEADER_TYPE);
  hdr->tb_id = swap_int(hdr->tb_id);
  hdr->aib_if_switch = swap_int(hdr->aib_if_switch);
  hdr->mb_rev = swap_int(hdr->mb_rev);
  hdr->mb_serial = swap_int(hdr->mb_serial);
  hdr->tb_rev = swap_int(hdr->tb_rev);
  hdr->tb_serial = swap_int(hdr->tb_serial);
  hdr->mb_xtal_freq = swap_int(hdr->mb_xtal_freq);
  hdr->scan_num = swap_uint(hdr->scan_num);
  hdr->ll_file_offset = swap_longlong(hdr->ll_file_offset);
  hdr->ll_file_size = swap_longlong(hdr->ll_file_size);
  hdr->length_of_integration = swap_double(hdr->length_of_integration);
  hdr->samp_rate = swap_double(hdr->samp_rate);
  hdr->ra_2000 = swap_double(hdr->ra_2000);
  hdr->dec_2000 = swap_double(hdr->dec_2000);
  hdr->tele_x = swap_double(hdr->tele_x);
  hdr->tele_y = swap_double(hdr->tele_y);
  hdr->tele_z = swap_double(hdr->tele_z);
  hdr->tele_inc = swap_double(hdr->tele_inc);
  hdr->Fclk = swap_double(hdr->Fclk);
  hdr->Har_Clk = swap_double(hdr->Har_Clk);
  hdr->bandwidth = swap_double(hdr->bandwidth);
  hdr->rf_lo = swap_double(hdr->rf_lo);
  hdr->max_dfb_freq = swap_double(hdr->max_dfb_freq);
  hdr->mjd_start = swap_longdouble(hdr->mjd_start);
  for (ii=0; ii<FB_CHAN_PER_BRD; ii++){
    hdr->dfb_sram_freqs[ii] = swap_float(hdr->dfb_sram_freqs[ii]);
  }
  for (ii=0; ii<MAX_HARRIS_TAPS; ii++){
    hdr->i_hcoef[ii] = swap_int(hdr->i_hcoef[ii]);
    hdr->q_hcoef[ii] = swap_int(hdr->q_hcoef[ii]);
  }
  for (ii=0; ii<MAXNUMCB; ii++){
    hdr->cb_id[ii] = swap_int(hdr->cb_id[ii]);
    hdr->cb_rev[ii] = swap_int(hdr->cb_rev[ii]);
    hdr->cb_serial[ii] = swap_int(hdr->cb_serial[ii]);
    hdr->cb_eprom_mode[ii] = swap_int(hdr->cb_eprom_mode[ii]);
  }
  for (ii=0; ii<MAX_NUM_MF_BOARDS; ii++){
    hdr->mf_rev[ii] = swap_int(hdr->mf_rev[ii]);
    hdr->mf_serial[ii] = swap_int(hdr->mf_serial[ii]);
    hdr->mf_filt_width[ii] = swap_double(hdr->mf_filt_width[ii]);
    hdr->mf_atten[ii] = swap_double(hdr->mf_atten[ii]);
  }
  for (ii=0; ii<MAX_NUM_LO_BOARDS; ii++){
    hdr->lo_rev[ii] = swap_int(hdr->lo_rev[ii]);
    hdr->lo_serial[ii] = swap_int(hdr->lo_serial[ii]);
    hdr->aib_los[ii] = swap_double(hdr->aib_los[ii]);
  }
  for (ii=0; ii<MAXNUMDFB; ii++){
    hdr->dfb_mixer_reg[ii] = swap_int(hdr->dfb_mixer_reg[ii]);
    hdr->dfb_conf_reg[ii] = swap_int(hdr->dfb_conf_reg[ii]);
    hdr->dfb_sram_addr_msb[ii] = swap_int(hdr->dfb_sram_addr_msb[ii]);
    hdr->dfb_id[ii] = swap_int(hdr->dfb_id[ii]);
    hdr->dfb_rev[ii] = swap_int(hdr->dfb_rev[ii]);
    hdr->dfb_serial[ii] = swap_int(hdr->dfb_serial[ii]);
    hdr->dfb_sun_program[ii] = swap_int(hdr->dfb_sun_program[ii]);
    hdr->dfb_eprom[ii] = swap_int(hdr->dfb_eprom[ii]);
    hdr->dfb_sram_addr[ii] = swap_int(hdr->dfb_sram_addr[ii]);
    hdr->dfb_har_addr[ii] = swap_int(hdr->dfb_har_addr[ii]);
    hdr->dfb_clip_adc_neg8[ii] = swap_int(hdr->dfb_clip_adc_neg8[ii]);
    hdr->dfb_shften_[ii] = swap_int(hdr->dfb_shften_[ii]);
    hdr->dfb_fwd_[ii] = swap_int(hdr->dfb_fwd_[ii]);
    hdr->dfb_rvrs_[ii] = swap_int(hdr->dfb_rvrs_[ii]);
    hdr->dfb_asymmetric[ii] = swap_int(hdr->dfb_asymmetric[ii]);
    hdr->dfb_i_dc[ii] = swap_double(hdr->dfb_i_dc[ii]);
    hdr->dfb_q_dc[ii] = swap_double(hdr->dfb_q_dc[ii]);
    hdr->dfb_gain[ii] = swap_double(hdr->dfb_gain[ii]);
  }
}


void calc_BPP_chans(BPP_SEARCH_HEADER *hdr)
/* Calculates freqs and ordering index table for BPP channels */
{
  int ii, n=0, dfb_chan, logical_board, regid, bid, nibble, nchans;
  double  f_aib, u_or_l, f_sram, fc;
  findex *findexarr;

  /* The following is probably a bad was to see if */
  /* we need to swap the endianess of the header.  */
  if (hdr->num_chans < 0 || hdr->num_chans > 2*MAXNUMCHAN){
    swapendian_BPP_header(hdr);
  }
  nchans = (hdr->mb_end_address / 2 - hdr->mb_start_address / 2 + 1) * \
    (hdr->mb_end_board - hdr->mb_start_board + 1) * 4;
  if (nchans > 2*MAXNUMCHAN){
    printf("Error:  nchans (%d) > 2*MAXNUMCHAN (%d) in bpp_calc_chans()\n\n",
	   nchans, 2*MAXNUMCHAN);
    exit(1);
  }
  /* Loop over (16-bit) regs per board. divide by 2's are to make them   */
  /* word addresses instead of byte addresses so we can index with them. */
  /* Normal modes will be regid = 0..3, 0..7, or 4..7                    */
  for (regid=hdr->mb_start_address/2; regid<=hdr->mb_end_address/2; regid++){
    /* Loop over each board */
    for (bid=hdr->mb_start_board; bid<=hdr->mb_end_board; bid++){
      /* Now find which LOGICAL CB we are reading */
      logical_board = -1;
      for (ii=0; ii<MAXNUMCB; ii++){
        if (bid==hdr->cb_id[ii]){
          logical_board = ii;
          break;
        }
      }
      if (logical_board==-1){
	printf("calc_BPP_chans() - logical_board not found");
	exit(1);
      }
      /* Assumes cabling so that LO0 feeds MF0,1 which feeds leftmost CB! */
      f_aib = hdr->aib_los[logical_board];
      /* Loop over 4 nibbles per reg */
      for (nibble=0; nibble<4; nibble++){
        dfb_chan = dfb_chan_lookup[regid][nibble];
        u_or_l = sideband_lookup[regid][nibble];
        f_sram = hdr->dfb_sram_freqs[dfb_chan];
	fc = f_aib + f_sram + u_or_l * hdr->bandwidth/4.0;
        chan_freqs[n] = (hdr->rf_lo + fc) / 1000000.0;
	n++;
      }
    }
  }

  /* produce lookup table which gives channels in order of descending freq */

  /* Produce a lookup table which gives channels in order of increasing freq */
  
  findexarr = (findex *)malloc(sizeof(findex) * nchans);
  for (ii=0; ii<nchans; ii++){
    findexarr[ii].freq = chan_freqs[ii];
    findexarr[ii].index = ii;    
    /*
    printf("%d  %f\n", findexarr[ii].index, findexarr[ii].freq);
    */
  }
  qsort(findexarr, nchans, sizeof(findex), compare_findex);
  for (ii=0; ii<nchans; ii++)
    chan_index[ii] = findexarr[ii].index;
#if 1
  {
    FILE *out;
    unsigned int *nridx;

    nridx  = (unsigned int *)malloc(nchans * sizeof(unsigned int));
    indexx(nchans,chan_freqs-1,nridx-1);
    out = fopen("dunc.txt", "w");
    for (ii=0; ii<nchans; ii++){
      fprintf(out, "%10.5f  %3d  %10.5f  %3d\n", chan_freqs[nridx[ii]-1], nridx[ii]-1, chan_freqs[chan_index[ii]], chan_index[ii]);
      chan_index[ii] = nridx[ii]-1;
    }
    fclose(out);
    free(nridx);
  }
#endif
  free(findexarr);

  /*
  for (ii=0; ii<nchans; ii++){
    printf("%3d  %3d  %f\n", ii, chan_index[ii], chan_freqs[chan_index[ii]]);
  }
  */

  /* Set the static variables */
  n = nchans / 2;
  if (hdr->cb_sum_polarizations)
    numchan_st = nchans;
  else
    numchan_st = nchans / 2;
  mid_freq_st = 0.5 * (chan_freqs[chan_index[n]] + chan_freqs[chan_index[n-1]]);
  ch1_freq_st = chan_freqs[chan_index[0]];
  if (hdr->cb_sum_polarizations)
    delta_freq_st = chan_freqs[chan_index[1]] - chan_freqs[chan_index[0]];
  else
    delta_freq_st = chan_freqs[chan_index[2]] - chan_freqs[chan_index[0]];
}


void BPP_hdr_to_inf(BPP_SEARCH_HEADER *hdr, infodata *idata)
/* Convert BPP header into an infodata structure */
{
  double MJD;
  char ctmp[80];

  strncpy(idata->object, hdr->target_name, 32);
  idata->ra_h = (int) floor(hdr->ra_2000 / 10000.0);
  idata->ra_m = (int) floor((hdr->ra_2000 - 
			     idata->ra_h * 10000) / 100.0);
  idata->ra_s = hdr->ra_2000 - idata->ra_h * 10000 - 
    idata->ra_m * 100;
  idata->dec_d = (int) floor(fabs(hdr->dec_2000) / 10000.0);
  idata->dec_m = (int) floor((fabs(hdr->dec_2000) - 
			      idata->dec_d * 10000) / 100.0);
  idata->dec_s = fabs(hdr->dec_2000) - idata->dec_d * 10000 - 
    idata->dec_m * 100;
  if (hdr->dec_2000 < 0.0)
    idata->dec_d = -idata->dec_d;
  strcpy(idata->telescope, "GBT");
  strcpy(idata->instrument, "BCPM1");
  idata->num_chan = numchan_st;
  idata->dt = hdr->samp_rate / 1000000.0;
  idata->N = hdr->length_of_integration / idata->dt;
  idata->chan_wid = fabs(delta_freq_st);
  idata->freqband = idata->num_chan * idata->chan_wid;
  idata->freq = ch1_freq_st;
  idata->fov = 1.2 * SOL * 3600.0 / (1000000.0 * idata->freq * 64 * DEGTORAD);
  MJD = UT_strings_to_MJD(hdr->date, hdr->start_time, 
			  &(idata->mjd_i), &(idata->mjd_f));
  idata->bary = 0;
  idata->numonoff = 0;
  strcpy(idata->band, "Radio");
  strcpy(idata->analyzer, "Scott Ransom");
  strcpy(idata->observer, "--");
  if (hdr->cb_sum_polarizations)
    sprintf(ctmp, "Polarizations were summed in hardware.");
  else
    sprintf(ctmp, "Polarizations were not summed in hardware.");
  sprintf(idata->notes, "Scan number %010d from tape %d, file %d.\n    Topo UTC Date (DDD:YYYY) & Time at file start = %s, %s\n    %s\n", 
	  hdr->scan_num, hdr->tape_num, hdr->tape_file_number, 
	  hdr->date, hdr->start_time, ctmp);
}


void get_BPP_file_info(FILE *files[], int numfiles, long long *N, 
		       int *ptsperblock, int *numchan, double *dt, 
		       double *T, infodata *idata, int output)
/* Read basic information into static variables and make padding      */
/* calculations for a set of BPP rawfiles that you want to patch      */
/* together.  N, numchan, dt, and T are return values and include all */
/* the files with the required padding.  If output is true, prints    */
/* a table showing a summary of the values.                           */
{
  int ii;
  char rawhdr[BPP_HEADER_SIZE];
  BPP_SEARCH_HEADER *header;

  if (numfiles > MAXPATCHFILES){
    printf("\nThe number of input files (%d) is greater than \n", numfiles);
    printf("   MAXPATCHFILES=%d.  Exiting.\n\n", MAXPATCHFILES);
    exit(0);
  }
  chkfread(rawhdr, BPP_HEADER_SIZE, 1, files[0]);
  header = (BPP_SEARCH_HEADER *)rawhdr;
  calc_BPP_chans(header);
#if 0
  {
    double *freq;
    int *nibble;

    for (ii=0; ii<NCHAN; ii++){
      gen_channel_mapping(header, &nibble, &freq, NULL);
      printf("%d  %f\n", nibble[ii], freq[ii]);
    }
  }
#endif
  BPP_hdr_to_inf(header, &idata_st[0]);
  BPP_hdr_to_inf(header, idata);
  *numchan = numchan_st = idata_st[0].num_chan;
  *ptsperblock = ptsperblk_st;
  numifs_st = header->num_chans / *numchan;
  if (numifs_st==1)
    padval = 7;
  else 
    padval = 14;
  bytesperpt_st = (numchan_st * numifs_st * 4) / 8;
  bytesperblk_st = ptsperblk_st * bytesperpt_st;
  numblks_st[0] = chkfilelen(files[0], bytesperblk_st);
  numpts_st[0] = numblks_st[0] * ptsperblk_st;
  N_st = numpts_st[0];
  dt_st = *dt = idata_st[0].dt;
  times_st[0] = numpts_st[0] * dt_st;
  mjds_st[0] = idata_st[0].mjd_i + idata_st[0].mjd_f;
  elapsed_st[0] = 0.0;
  startblk_st[0] = 1;
  endblk_st[0] = (double) numpts_st[0] / ptsperblk_st;
  padpts_st[0] = padpts_st[numfiles-1] = 0;
  for (ii=1; ii<numfiles; ii++){
    chkfread(rawhdr, BPP_HEADER_SIZE, 1, files[ii]);
    header = (BPP_SEARCH_HEADER *)rawhdr;
    calc_BPP_chans(header);
    BPP_hdr_to_inf(header, &idata_st[ii]);
    if (idata_st[ii].num_chan != numchan_st){
      printf("Number of channels (file %d) is not the same!\n\n", ii+1);
    }
    if (idata_st[ii].dt != dt_st){
      printf("Sample time (file %d) is not the same!\n\n", ii+1);
    }
    numblks_st[ii] = chkfilelen(files[ii], bytesperblk_st);
    numpts_st[ii] = numblks_st[ii] * ptsperblk_st;
    times_st[ii] = numpts_st[ii] * dt_st;
    mjds_st[ii] = idata_st[ii].mjd_i + idata_st[ii].mjd_f;
    elapsed_st[ii] = mjd_sec_diff(idata_st[ii].mjd_i, idata_st[ii].mjd_f,
				  idata_st[ii-1].mjd_i, idata_st[ii-1].mjd_f);
    padpts_st[ii-1] = (long long)((elapsed_st[ii]-times_st[ii-1]) / 
				  dt_st + 0.5);
    elapsed_st[ii] += elapsed_st[ii-1];
    N_st += numpts_st[ii] + padpts_st[ii-1];
    startblk_st[ii] = (double) (N_st - numpts_st[ii]) /
      ptsperblk_st + 1;
    endblk_st[ii] = (double) (N_st) / ptsperblk_st;
  }
  padpts_st[numfiles-1] = ((long long) ceil(endblk_st[numfiles-1]) *
                           ptsperblk_st - N_st);
  N_st += padpts_st[numfiles-1];
  *N = N_st;
  *T = T_st = N_st * dt_st;
  currentfile = currentblock = 0;
  if (output){
    printf("  Number of files = %d\n", numfiles);
    printf("     Points/block = %d\n", ptsperblk_st);
    printf("  Num of channels = %d\n", numchan_st);
    printf(" Total points (N) = %lld\n", N_st);
    printf(" Sample time (dt) = %-14.14g\n", dt_st);
    printf("   Total time (s) = %-14.14g\n\n", T_st);
    printf("File  Start Block    Last Block     Points      Elapsed (s)      Time (s)            MJD           Padding\n");
    printf("----  ------------  ------------  ----------  --------------  --------------  ------------------  ----------\n");
    for (ii=0; ii<numfiles; ii++)
      printf("%2d    %12.11g  %12.11g  %10lld  %14.13g  %14.13g  %17.12f  %10lld\n",
             ii+1, startblk_st[ii], endblk_st[ii], numpts_st[ii],
             elapsed_st[ii], times_st[ii], mjds_st[ii], padpts_st[ii]);
    printf("\n");
  }
}


void BPP_update_infodata(int numfiles, infodata *idata)
/* Update the onoff bins section in case we used multiple files */
{
  
  int ii, index=2;

  idata->N = N_st;
  if (numfiles==1 && padpts_st[0]==0){
    idata->numonoff = 0;
    return;
  }
  /* Determine the topocentric onoff bins */
  idata->numonoff = 1;
  idata->onoff[0] = 0.0;
  idata->onoff[1] = numpts_st[0] - 1.0;
  for (ii=1; ii<numfiles; ii++){
    if (padpts_st[ii-1]){
      idata->onoff[index] = idata->onoff[index-1] + padpts_st[ii-1];
      idata->onoff[index+1] = idata->onoff[index] + numpts_st[ii];
      idata->numonoff++;
      index += 2;
    } else {
      idata->onoff[index-1] += numpts_st[ii];
    }
  }
  if (padpts_st[numfiles-1]){
    idata->onoff[index] = idata->onoff[index-1] + padpts_st[numfiles-1];
    idata->onoff[index+1] = idata->onoff[index];
    idata->numonoff++;
  }
}


int skip_to_BPP_rec(FILE *infiles[], int numfiles, int rec)
/* This routine skips to the record 'rec' in the input files   */
/* *infiles.  *infiles contains 4 bit digitized data from the  */
/* BCPM1 backend at the GBT.  Returns the record skipped to.   */
{
  double floor_blk;
  int filenum=0;
 
  if (rec < startblk_st[0])
    rec += (startblk_st[0] - 1);
  if (rec > 0 && rec < endblk_st[numfiles-1]){
 
    /* Find which file we need */
    while (rec > endblk_st[filenum])
      filenum++;
 
    currentblock = rec - 1;
    shiftbuffer = 1;
    floor_blk = floor(startblk_st[filenum]);
 
    /* Set the data buffer to all padding just in case */
    memset(databuffer, padval, 2*MAXDATLEN);
 
    /* Warning:  I'm not sure if the following is correct. */
    /*   If really needs accurate testing to see if my     */
    /*   offsets are correct.  Bottom line, don't trust    */
    /*   a TOA determined using the following!             */
 
    if (rec < startblk_st[filenum]){  /* Padding region */
      currentfile = filenum-1;
      chkfileseek(infiles[currentfile], 0, 1, SEEK_END);
      bufferpts = padpts_st[currentfile] % ptsperblk_st;
      padnum = ptsperblk_st * (rec - endblk_st[currentfile] - 1);
      /*
      printf("Padding:  currentfile = %d  bufferpts = %d  padnum = %d\n",
             currentfile, bufferpts, padnum);
      */
    } else {  /* Data region */
      currentfile = filenum;
      chkfileseek(infiles[currentfile], rec - startblk_st[filenum],
                  bytesperblk_st, SEEK_CUR);
      bufferpts = (int)((startblk_st[filenum] - floor_blk) * ptsperblk_st + 0.5);
      padnum = 0;
      /*
      printf("Data:  currentfile = %d  bufferpts = %d  padnum = %d\n",
             currentfile, bufferpts, padnum);
      */
    }
 
  } else {
    printf("\n rec = %d out of range in skip_to_BPP_rec()\n", rec);
    exit(1);
  }
  return rec;
}


void print_BPP_hdr(BPP_SEARCH_HEADER *hdr)
/* Output a BPP header in human readable form */
{
  int mjd_i;
  double mjd_d;

  calc_BPP_chans(hdr);
  printf("\n'%s' (version %d)\n", hdr->head, hdr->header_version);
  printf("                     Target = %s\n", hdr->target_name);
  if (strlen(hdr->obs_group))
    printf("                Observed by = %s\n", hdr->obs_group);
  printf("   Scan number (DDDYYYY###) = %010d\n", hdr->scan_num);
  printf("           Scan file number = %d\n", hdr->scan_file_number);
  /*
  printf("          File size (bytes) = %d\n", hdr->file_size);
  */
  printf("                Tape number = %d\n", hdr->tape_num);
  printf("           Tape file number = %d\n", hdr->tape_file_number);
  printf("       LMST in sec since 0h = %d\n", hdr->lmst);
  printf("        UTC date (DDD:YYYY) = %s\n", hdr->date);
  printf("        UTC time (HH:MM:SS) = %s\n", hdr->start_time);
  printf("             MJD start time = %.11f\n", 
	 UT_strings_to_MJD(hdr->date, hdr->start_time, &mjd_i, &mjd_d));
  printf("    RA (J2000, HHMMSS.SSSS) = %.4f\n", hdr->ra_2000);
  printf("   DEC (J2000, DDMMSS.SSSS) = %.4f\n", hdr->dec_2000);
  printf("     Integration length (s) = %.17g\n", hdr->length_of_integration);
  printf("           Sample time (us) = %.17g\n", hdr->samp_rate);
  if (hdr->bit_mode==4)
    printf("               Channel mode = Powers\n");
  else if (hdr->bit_mode==-4)
    printf("               Channel mode = Direct voltages\n");
  else
    printf("        UNKNOWN CHANNEL MODE!!\n");
  if (hdr->cb_sum_polarizations){
    printf("      Polarizations summed? = Yes\n");
    printf("         Number of channels = %d\n", hdr->num_chans);
    printf("    Overall bandwidth (MHz) = %.17g\n", 
	   hdr->num_chans * delta_freq_st);
  } else {
    printf("      Polarizations summed? = No\n");
    printf("         Number of channels = %d x 2 IFs\n", hdr->num_chans/2);
    printf("    Overall bandwidth (MHz) = %.17g\n", 
	   0.5 * hdr->num_chans * delta_freq_st);
  }
  printf("    Channel bandwidth (MHz) = %.17g\n", delta_freq_st);
  printf("  Lowest channel freq (MHz) = %.17g\n", ch1_freq_st);
  printf("          Middle freq (MHz) = %.17g\n", mid_freq_st);
  printf("\n");
}


int read_BPP_rawblock(FILE *infiles[], int numfiles, 
		      unsigned char *data, int *padding)
/* This routine reads a single record from the          */
/* input files *infiles which contain 4 bit digitized   */
/* data from the BCPM1 pulsar backend at the GBT.       */
/* A BPP record is ptsperblk_st*numchan_st*4 bits long. */
/* *data must be bytesperblk_st bytes long.  If padding */
/* is returned as 1, then padding was added and         */
/* statistics should not be calculated.                 */
{
  int offset=0, numtopad=0;
  unsigned char *dataptr;

  /* If our buffer array is offset from last time */
  /* copy the second part into the first.         */

  if (bufferpts && shiftbuffer){
    offset = bufferpts * bytesperpt_st;
    memcpy(databuffer, databuffer + bytesperblk_st, offset);
    dataptr = databuffer + offset;
  } else {
    dataptr = data;
  }
  shiftbuffer=1;

  /* Make sure our current file number is valid */

  if (currentfile >= numfiles)
    return 0;

  /* First, attempt to read data from the current file */
  
  if (fread(dataptr, bytesperblk_st, 1, infiles[currentfile])){ /* Got Data */
    *padding = 0;
    /* Put the new data into the databuffer if needed */
    if (bufferpts){
      memcpy(data, dataptr, bytesperblk_st);
    }
    currentblock++;
    return 1;
  } else { /* Didn't get data */
    if (feof(infiles[currentfile])){  /* End of file? */
      numtopad = padpts_st[currentfile] - padnum;
      if (numtopad){  /* Pad the data? */
	*padding = 1;
	if (numtopad >= ptsperblk_st - bufferpts){  /* Lots of padding */
	  if (bufferpts){  /* Buffer the padding? */
	    /* Add the amount of padding we need to */
	    /* make our buffer offset = 0           */
	    numtopad = ptsperblk_st - bufferpts;
	    memset(dataptr, padval, numtopad * bytesperpt_st);
	    /* Copy the new data/padding into the output array */
	    memcpy(data, databuffer, bytesperblk_st);
	    bufferpts = 0;
	  } else {  /* Add a full record of padding */
	    numtopad = ptsperblk_st;
	    memset(data, padval, bytesperblk_st);
	  }
	  padnum += numtopad;
	  currentblock++;
	  /* If done with padding reset padding variables */
	  if (padnum==padpts_st[currentfile]){
	    padnum = 0;
	    currentfile++;
	  }
	  return 1;
	} else {  /* Need < 1 block (or remaining block) of padding */
	  int pad;
	  /* Add the remainder of the padding and */
	  /* then get a block from the next file. */
          memset(databuffer + bufferpts * bytesperpt_st, 
		 padval, numtopad * bytesperpt_st);
	  padnum = 0;
	  currentfile++;
	  shiftbuffer = 0;
	  bufferpts += numtopad;
	  return read_BPP_rawblock(infiles, numfiles, data, &pad);
	}
      } else {  /* No padding needed.  Try reading the next file */
	currentfile++;
	shiftbuffer = 0;
	return read_BPP_rawblock(infiles, numfiles, data, padding);
      }
    } else {
      printf("\nProblem reading record from BPP data file:\n");
      printf("   currentfile = %d, currentblock = %d.  Exiting.\n",
	     currentfile, currentblock);
      exit(1);
    }
  }
}


int read_BPP_rawblocks(FILE *infiles[], int numfiles, 
		       unsigned char rawdata[], int numblocks,
		       int *padding)
     /* This routine reads numblocks BPP records from the input  */
/* files *infiles.  The 4-bit data is returned in rawdata   */
/* which must have a size of numblocks * bytesperblk_st.    */
/* The number  of blocks read is returned.                  */
/* If padding is returned as 1, then padding was added      */
/* and statistics should not be calculated                  */
{
  int ii, retval=0, pad, numpad=0;
  
  *padding = 0;
  for (ii=0; ii<numblocks; ii++){
    retval += read_BPP_rawblock(infiles, numfiles, 
				rawdata + ii * bytesperblk_st, &pad);
    if (pad)
      numpad++;
  }
  /* Return padding 'true' if more than */
  /* half of the blocks are padding.    */
  /* 
     if (numpad > numblocks / 2)
        *padding = 1;
  */
  /* Return padding 'true' if any block was padding */
  if (numpad) 
    *padding = 1;
  return retval;
}


int read_BPP(FILE *infiles[], int numfiles, float *data, 
	     int numpts, double *dispdelays, int *padding, 
	     int *maskchans, int *nummasked, mask *obsmask, 
	     BPP_ifs ifs)
/* This routine reads numpts from the BPP raw input    */
/* files *infiles.  These files contain 4-bit data     */
/* from the BCPM1 backend at the GBT.  Time delays     */
/* and a mask are applied to each channel.  It returns */
/* the # of points read if successful, 0 otherwise.    */
/* If padding is returned as 1, then padding was       */
/* added and statistics should not be calculated.      */
/* maskchans is an array of length numchans contains   */
/* a list of the number of channels that were masked.  */
/* ifs is which ifs to return (assuming both IFs have  */
/* been recorded. Legal values are IF0, IF1, SUMIFS.   */
/* The # of channels masked is returned in nummasked.  */
/* obsmask is the mask structure to use for masking.   */
{
  int ii, jj, numread=0, offset, sampperblk;
  double starttime=0.0;
  static unsigned char *tempzz, *raw, *rawdata1, *rawdata2; 
  static unsigned char *currentdata, *lastdata;
  static int firsttime=1, numblocks=1, allocd=0, mask=0;
  static double duration=0.0, timeperblk=0.0;

  /* The x2 comes from 4-bits/pt */
  sampperblk = bytesperblk_st * 2;
  if (numifs_st==2 && (ifs==IF0 || ifs==IF1)) 
    padval = 7;
  *nummasked = 0;
  if (firsttime) {
    if (numpts % ptsperblk_st){
      printf("numpts must be a multiple of %d in read_BPP()!\n",
	     ptsperblk_st);
      exit(1);
    } else
      numblocks = numpts / ptsperblk_st;
    
    if (obsmask->numchan) mask = 1;
    raw  = gen_bvect(numblocks * bytesperblk_st);
    rawdata1 = gen_bvect(numblocks * sampperblk);
    rawdata2 = gen_bvect(numblocks * sampperblk);
    allocd = 1;
    timeperblk = ptsperblk_st * dt_st;
    duration = numblocks * timeperblk;
    
    numread = read_BPP_rawblocks(infiles, numfiles, raw, 
				 numblocks, padding);
    if (numread != numblocks && allocd){
      printf("Problem reading the raw BPP data file.\n\n");
      free(raw);
      free(rawdata1);
      free(rawdata2);
      allocd = 0;
      return 0;
    }
    
    currentdata = rawdata1;
    lastdata = rawdata2;

    if (mask){
      starttime = currentblock * timeperblk;
      *nummasked = check_mask(starttime, duration, obsmask, maskchans);
      if (*nummasked==-1) /* If all channels are masked */
	memset(raw, padval, numblocks * sampperblk);
    }

    if (numifs_st==2){
      /* Choosing a single IF */
      if (ifs==IF0 || ifs==IF1)
	for (ii=0; ii<numpts; ii++)
	  convert_BPP_one_IF(raw + ii * bytesperpt_st, 
			     currentdata + ii * numchan_st, ifs);
      /* Sum the IFs */
      else
	for (ii=0; ii<numpts; ii++)
	  convert_BPP_sum_IFs(raw + ii * bytesperpt_st, 
			      currentdata + ii * numchan_st);
    } else {
      /* Select the already summed IFs */
      for (ii=0; ii<numpts; ii++)
	convert_BPP_point(raw + ii * bytesperpt_st, 
			  currentdata + ii * numchan_st);
    }

    if (*nummasked > 0){ /* Only some of the channels are masked */
      for (ii=0; ii<*nummasked; ii++)
	chanmask[ii] = maskchans[ii] & 0x01;
      for (ii=0; ii<numpts; ii++){
	offset = ii * numchan_st;
	for (jj=0; jj<*nummasked; jj++)
	  currentdata[offset+maskchans[jj]] = chanmask[jj];
      }
    }

    SWAP(currentdata, lastdata);
    firsttime=0;
  }
  
  /* Read, convert and de-disperse */
  
  if (allocd){
    numread = read_BPP_rawblocks(infiles, numfiles, raw, 
				 numblocks, padding);

    if (mask){
      starttime = currentblock * timeperblk;
      *nummasked = check_mask(starttime, duration, obsmask, maskchans);
      if (*nummasked==-1) /* If all channels are masked */
	memset(raw, padval, numblocks * sampperblk);
    }

    if (numifs_st==2){
      /* Choosing a single IF */
      if (ifs==IF0 || ifs==IF1)
	for (ii=0; ii<numpts; ii++)
	  convert_BPP_one_IF(raw + ii * bytesperpt_st, 
			     currentdata + ii * numchan_st, ifs);
      /* Sum the IFs */
      else
	for (ii=0; ii<numpts; ii++)
	  convert_BPP_sum_IFs(raw + ii * bytesperpt_st, 
			      currentdata + ii * numchan_st);
    } else {
      /* Select the already summed IFs */
      for (ii=0; ii<numpts; ii++)
	convert_BPP_point(raw + ii * bytesperpt_st, 
			  currentdata + ii * numchan_st);
    }

    if (*nummasked > 0){ /* Only some of the channels are masked */
      for (ii=0; ii<*nummasked; ii++)
	chanmask[ii] = maskchans[ii] & 0x01;
      for (ii=0; ii<numpts; ii++){
	offset = ii * numchan_st;
	for (jj=0; jj<*nummasked; jj++)
	  currentdata[offset+maskchans[jj]] = chanmask[jj];
      }
    }

    dedisp(currentdata, lastdata, numpts, numchan_st, dispdelays, data);
    SWAP(currentdata, lastdata);

    if (numread != numblocks){
      free(raw);
      free(rawdata1);
      free(rawdata2);
      allocd = 0;
    }
    return numread * ptsperblk_st;
  } else {
    return 0;
  }
}

#if 0

void get_BPP_channel(int channum, float chandat[], 
		     unsigned char rawdata[], int numblocks,
		     BPP_ifs ifs)
/* Return the values for channel 'channum' of a block of       */
/* 'numblocks' raw BPP data stored in 'rawdata' in 'chandat'.  */
/* 'rawdata' should have been initialized using                */
/* read_BPP_rawblocks(), and 'chandat' must have at least      */
/* 'numblocks' * 'ptsperblk_st' spaces.                        */
/* Channel 0 is assumed to be the lowest freq channel.         */
/* The different IFs are handled as standard channel numbers   */
/* with IF0 going from 0-numchan/2-1 and IF1 from there up     */
/* If 'ifs' equals SUMIFS then the channels go from            */
/* 0-numchan_st-1 as per normal.                               */
{
  int ii, bit;

  if (ifs==IF0)

  if (channum > numchan_st || channum < 0){
    printf("\nchannum = %d is out of range in get_BPP_channel()!\n\n",
	   channum);
    exit(1);
  }
  bit = (decreasing_freqs_st) ? numchan_st - 1 - channum : channum;
  for (ii=0; ii<numblocks*ptsperblk_st; ii++)
    chandat[ii] = (float) GET_BIT(rawdata + ii * bytesperpt_st, bit);
}

#endif


int read_BPP_subbands(FILE *infiles[], int numfiles, float *data, 
		      double *dispdelays, int numsubbands, 
		      int transpose, int *padding, 
		      int *maskchans, int *nummasked, mask *obsmask,
		      BPP_ifs ifs)
/* This routine reads a record from the input files *infiles[]   */
/* which contain data from the BPP system.  The routine uses    */
/* dispersion delays in 'dispdelays' to de-disperse the data     */
/* into 'numsubbands' subbands.  It stores the resulting data    */
/* in vector 'data' of length 'numsubbands' * 'ptsperblk_st'.    */
/* The low freq subband is stored first, then the next highest   */
/* subband etc, with 'ptsperblk_st' floating points per subband. */
/* It returns the # of points read if succesful, 0 otherwise.    */
/* If padding is returned as 1, then padding was added and       */
/* statistics should not be calculated.  'maskchans' is an array */
/* of length numchans which contains a list of the number of     */
/* channels that were masked.  The # of channels masked is       */
/* returned in 'nummasked'.  'obsmask' is the mask structure     */
/* to use for masking.  If 'transpose'==0, the data will be kept */
/* in time order instead of arranged by subband as above.        */
/* ifs is which ifs to return (assuming both IFs have been       */
/* recorded. Legal values are IF0, IF1, SUMIFS.                  */
{
  int ii, jj, numread, trtn, offset;
  double starttime=0.0;
  static unsigned char *raw, *tempzz, bytepadval;
  static unsigned char rawdata1[MAXDATLEN], rawdata2[MAXDATLEN]; 
  static unsigned char *currentdata, *lastdata, *move;
  static int firsttime=1, move_size=0, mask=0;
  static double timeperblk=0.0;
  
  *nummasked = 0;
  if (firsttime) {
    if (!read_BPP_rawblock(infiles, numfiles, raw, padding)){
      printf("Problem reading the raw BPP data file.\n\n");
      return 0;
    }
    if (obsmask->numchan) mask = 1;
    move_size = (ptsperblk_st + numsubbands) / 2;
    move = gen_bvect(move_size);
    raw = gen_bvect(bytesperblk_st);
    currentdata = rawdata1;
    lastdata = rawdata2;
    timeperblk = ptsperblk_st * dt_st;
    /* XXXXXXXXXXXXXXXXX */
    bytepadval = (padval << 4) | padval;
    if (mask){
      starttime = currentblock * timeperblk;
      *nummasked = check_mask(starttime, timeperblk, obsmask, maskchans);
      if (*nummasked==-1) /* If all channels are masked */
	memset(raw, padval, bytesperblk_st);
    }
    for (ii=0; ii<ptsperblk_st; ii++)
      convert_BPP_point(raw + ii * bytesperpt_st, 
			 currentdata + ii * numchan_st);
    if (*nummasked > 0){ /* Only some of the channels are masked */
      for (ii=0; ii<*nummasked; ii++)
	chanmask[ii] = maskchans[ii] & 0x01;
      for (ii=0; ii<ptsperblk_st; ii++){
	offset = ii * numchan_st;
	for (jj=0; jj<*nummasked; jj++)
	  currentdata[offset+maskchans[jj]] = chanmask[jj];
      }
    }
    SWAP(currentdata, lastdata);
    firsttime=0;
  }

  /* Read, convert and de-disperse */

  numread = read_BPP_rawblock(infiles, numfiles, raw, padding);
  if (mask){
    starttime = currentblock * timeperblk;
    *nummasked = check_mask(starttime, timeperblk, obsmask, maskchans);
    if (*nummasked==-1) /* If all channels are masked */
      memset(raw, padval, bytesperblk_st);
  }
  for (ii=0; ii<ptsperblk_st; ii++)
    convert_BPP_point(raw + ii * bytesperpt_st, 
		       currentdata + ii * numchan_st);
  if (*nummasked > 0){ /* Only some of the channels are masked */
    for (ii=0; ii<*nummasked; ii++)
      chanmask[ii] = maskchans[ii] & 0x01;
    for (ii=0; ii<ptsperblk_st; ii++){
      offset = ii * numchan_st;
      for (jj=0; jj<*nummasked; jj++)
	currentdata[offset+maskchans[jj]] = chanmask[jj];
    }
  }
  dedisp_subbands(currentdata, lastdata, ptsperblk_st, numchan_st, 
		  dispdelays, numsubbands, data);
  SWAP(currentdata, lastdata);

  /* Transpose the data into vectors in the result array */

  if (transpose){
    if ((trtn = transpose_float(data, ptsperblk_st, numsubbands,
				move, move_size))<0)
      printf("Error %d in transpose_float().\n", trtn);
  }
  if (numread)
    return ptsperblk_st;
  else
    return 0;
}

void convert_BPP_one_IF(unsigned char *rawdata, unsigned char *bytes,
			BPP_ifs ifs)
/* This routine converts a single IF from 4-bit digitized */
/* data of two IFs into an array of 'numchan' bytes.      */
{
  int ii, jj=0, *indexptr;
  unsigned char *rawdataptr, ifbytes[2*MAXNUMCHAN];

  rawdataptr = rawdata;
  indexptr = chan_index;
  for (ii=0; ii<numchan_st*2; ii++) 
    ifbytes[ii] = 0;
  for (ii=0; ii<numchan_st; ii++, rawdataptr++){
    ifbytes[*indexptr++] += (*rawdataptr >> 4);
    ifbytes[*indexptr++] += (*rawdataptr & 0x0F);
  }
  if (ifs==IF1)
    jj = numchan_st;
  printf("pts = asarray([");
  for (ii=0; ii<2*numchan_st; ii++)
    printf("%d, ", ifbytes[ii]);
  printf("]\n");
  for (ii=0; ii<numchan_st; ii++, jj++)
    bytes[ii] = ifbytes[jj];
}


void convert_BPP_sum_IFs(unsigned char *rawdata, unsigned char *bytes)
/* This routine converts 4-bit digitized data for 2 IFs */
/* into a summed IF array of 'numchan' bytes.           */
{
  int ii, jj, *indexptr;
  unsigned char *rawdataptr, ifbytes[2*MAXNUMCHAN];

  rawdataptr = rawdata;
  indexptr = chan_index;
  for (ii=0; ii<numchan_st*2; ii++) 
    ifbytes[ii] = 0;
  for (ii=0; ii<numchan_st; ii++, *rawdataptr++){
    ifbytes[*indexptr++] += (*rawdataptr & 0x0F);
    ifbytes[*indexptr++] += (*rawdataptr >> 0x04);
  }
  for (ii=0, jj=numchan_st; ii<numchan_st; ii++, jj++)
    bytes[ii] = ifbytes[ii] + ifbytes[jj];
}


void convert_BPP_point(unsigned char *rawdata, unsigned char *bytes)
/* This routine converts 4-bit digitized power data */
/* into an array of 'numchan' bytes.                */
{
  int ii, *indexptr;
  unsigned char *rawdataptr;

  rawdataptr = rawdata;
  indexptr = chan_index;
  for (ii=0; ii<numchan_st/2; ii++, *rawdataptr++){
    bytes[*indexptr++] = (*rawdataptr & 0x0F);
    bytes[*indexptr++] = (*rawdataptr >> 0x04);
  }
}

#undef SWAP
#define NRANSI
#define SWAP(a,b) itemp=(a);(a)=(b);(b)=itemp;
#define M 7
#define NSTACK 50
#define NR_END 1
#define FREE_ARG char*

int *ivector(long nl, long nh)
/* allocate an int vector with subscript range v[nl..nh] */
{
        int *v;
 
        v=(int *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(int)));
        if (!v){
	  printf("allocation failure in ivector()");
	  exit(1);
	}
        return v-nl+NR_END;
}

void free_ivector(int *v, long nl, long nh)
/* free an int vector allocated with ivector() */
{
        free((FREE_ARG) (v+nl-NR_END));
}
 
void indexx(unsigned int n, double arr[], unsigned int indx[])
{
        unsigned int i,indxt,ir=n,itemp,j,k,l=1;
        int jstack=0,*istack;
        double a;
 
        istack=ivector(1,NSTACK);
        for (j=1;j<=n;j++) indx[j]=j;
        for (;;) {
                if (ir-l < M) {
                        for (j=l+1;j<=ir;j++) {
                                indxt=indx[j];
                                a=arr[indxt];
                                for (i=j-1;i>=1;i--) {
                                        if (arr[indx[i]] <= a) break;
                                        indx[i+1]=indx[i];
                                }
                                indx[i+1]=indxt;
                        }
                        if (jstack == 0) break;
                        ir=istack[jstack--];
                        l=istack[jstack--];
                } else {
                        k=(l+ir) >> 1;
                        SWAP(indx[k],indx[l+1]);
                        if (arr[indx[l+1]] > arr[indx[ir]]) {
                                SWAP(indx[l+1],indx[ir])
                        }
                        if (arr[indx[l]] > arr[indx[ir]]) {
                                SWAP(indx[l],indx[ir])
                        }
                        if (arr[indx[l+1]] > arr[indx[l]]) {
                                SWAP(indx[l+1],indx[l])
                        }
                        i=l+1;
                        j=ir;
                        indxt=indx[l];
                        a=arr[indxt];
                        for (;;) {
                                do i++; while (arr[indx[i]] < a);
                                do j--; while (arr[indx[j]] > a);
                                if (j < i) break;
                                SWAP(indx[i],indx[j])
                        }
                        indx[l]=indx[j];
                        indx[j]=indxt;
                        jstack += 2;
                        if (jstack > NSTACK){
			  printf("NSTACK too small in indexx.");
			  exit(1);
			}
                        if (ir-i+1 >= j-l) {
                                istack[jstack]=ir;
                                istack[jstack-1]=i;
                                ir=j-1;
                        } else {
                                istack[jstack]=j-1;
                                istack[jstack-1]=l;
                                l=i;
                        }
                }
        }
        free_ivector(istack,1,NSTACK);
}

#undef M
#undef NSTACK
#undef SWAP
#undef NRANSI
