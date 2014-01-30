#include "levmar-2.6/levmar.h"
#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// some internal details of the modules we're trying to optimize.
// these are reproduced here because we want to stick to the specific version unless
// the rest of this code is updated, too.

// define this to optimize for monochrome images
#define USE_MONOCHROME
// #define USE_EXPOSURE
// #define USE_AB_CURVES


// exposure
// ======================================================================
static const int exposure_version = 2;

typedef struct dt_iop_exposure_params_t
{
  float black, exposure, gain;
}
dt_iop_exposure_params_t;


// tonecurve
// ======================================================================
static const int tonecurve_version = 4;
#define DT_IOP_TONECURVE_MAXNODES 20

typedef struct dt_iop_tonecurve_node_t
{
  float x;
  float y;
}
dt_iop_tonecurve_node_t;

typedef struct dt_iop_tonecurve_params_t
{
  dt_iop_tonecurve_node_t tonecurve[3][DT_IOP_TONECURVE_MAXNODES];  // three curves (L, a, b) with max number of nodes
  int tonecurve_nodes[3];
  int tonecurve_type[3];
  int tonecurve_autoscale_ab;
  int tonecurve_preset;
  int tonecurve_unbound_ab;
}
dt_iop_tonecurve_params_t;

// color correction
// ======================================================================
static const int colorcorrection_version = 1;

typedef struct dt_iop_colorcorrection_params_t
{
  float hia, hib, loa, lob, saturation;
}
dt_iop_colorcorrection_params_t;


// color zones
// ======================================================================
static const int colorzones_version = 3;
#define DT_IOP_COLORZONES_BANDS 8

typedef enum dt_iop_colorzones_channel_t
{
  DT_IOP_COLORZONES_L = 0,
  DT_IOP_COLORZONES_C = 1,
  DT_IOP_COLORZONES_h = 2
}
dt_iop_colorzones_channel_t;

typedef struct dt_iop_colorzones_params_t
{
  int32_t channel;
  float equalizer_x[3][DT_IOP_COLORZONES_BANDS], equalizer_y[3][DT_IOP_COLORZONES_BANDS];
  float strength;
}
dt_iop_colorzones_params_t;

// monochrome
// ======================================================================
static const int monochrome_version = 2;

typedef struct dt_iop_monochrome_params_t
{
  float a, b, size, highlights;
}
dt_iop_monochrome_params_t;

// ======================================================================

typedef struct module_params_t
{
  dt_iop_exposure_params_t exp;
  dt_iop_tonecurve_params_t curve;
  dt_iop_colorcorrection_params_t corr;
  dt_iop_colorzones_params_t zones;
  dt_iop_monochrome_params_t mono;
}
module_params_t;

static inline module_params_t *init_params()
{
  module_params_t *m = (module_params_t *)malloc(sizeof(module_params_t));
  memset(m, 0, sizeof(module_params_t));

  // exposure:
  m->exp.black = 0.0f;
  m->exp.exposure = 0.0f;
  m->exp.gain = 1.0f;

  // curve:
  for(int k=0;k<3;k++) m->curve.tonecurve_type[k] = 2; // MONOTONE_HERMITE
  for(int k=0;k<3;k++) m->curve.tonecurve_nodes[k] = 9; // enough i think.
  for(int i=0;i<3;i++)
    for(int k=0;k<9;k++)
    {
      m->curve.tonecurve[i][k].x = k/8.0f;
      m->curve.tonecurve[i][k].y = k/8.0f; // start at identity
    }
#ifdef USE_AB_CURVES
  m->curve.tonecurve_autoscale_ab = 0;
#else
  m->curve.tonecurve_autoscale_ab = 1;
#endif
  m->curve.tonecurve_preset = 0;
  m->curve.tonecurve_unbound_ab = 0;

  // color correction
  m->corr.saturation = 1.0f; // the rest is 0

  // color zones
  for(int ch=0; ch<3; ch++)
    for(int k=0; k<DT_IOP_COLORZONES_BANDS; k++)
    {
      m->zones.equalizer_x[ch][k] = k/(DT_IOP_COLORZONES_BANDS-1.0f);
      m->zones.equalizer_y[ch][k] = 0.5f;
    }
  m->zones.strength = 0.0;
  m->zones.channel = DT_IOP_COLORZONES_h;

  // monochrome:
  m->mono.a = 0.f;
  m->mono.b = 0.f;
  m->mono.size = 2.f;
  m->mono.highlights= 0.f;

  return m;
}

static inline int params2float(const module_params_t *m, float *f)
{
  int j = 0;

#ifdef USE_EXPOSURE
  f[j++] = m->exp.black;
  f[j++] = m->exp.exposure;
#endif

#ifdef USE_AB_CURVES
  for(int i=0;i<3;i++)
#else
    const int i = 0;
#endif
    for(int k=0;k<9;k++)
      f[j++] = m->curve.tonecurve[i][k].y;

  f[j++] = m->corr.hia;
  f[j++] = m->corr.hib;
  f[j++] = m->corr.loa;
  f[j++] = m->corr.lob;
  f[j++] = m->corr.saturation;

  for(int ch=0; ch<3; ch++)
    for(int k=0; k<DT_IOP_COLORZONES_BANDS; k++)
      f[j++] = m->zones.equalizer_y[ch][k];
  f[j++] = m->zones.strength;
  // TODO: shall we mutate this? probably not.
  // f[j++] = m->zones.channel+.5f;

#ifdef USE_MONOCHROME
  f[j++] = m->mono.a;
  f[j++] = m->mono.b;
  f[j++] = m->mono.size;
  f[j++] = m->mono.highlights;
#endif

  return j;
}

static inline int float2params(const float *f, module_params_t *m)
{
  int j = 0;

#ifdef USE_EXPOSURE
  m->exp.black = f[j++];
  m->exp.exposure = f[j++];
#endif

#ifdef USE_AB_CURVES
  for(int i=0;i<3;i++)
#else
    const int i = 0;
#endif
    for(int k=0;k<9;k++)
      m->curve.tonecurve[i][k].y = f[j++];

  m->corr.hia = f[j++];
  m->corr.hib = f[j++];
  m->corr.loa = f[j++];
  m->corr.lob = f[j++];
  m->corr.saturation = f[j++];

  for(int ch=0; ch<3; ch++)
    for(int k=0; k<DT_IOP_COLORZONES_BANDS; k++)
      m->zones.equalizer_y[ch][k] = f[j++];
  m->zones.strength = f[j++];
  // TODO: shall we mutate this? probably not.
  // m->zones.channel = (int)roundf(f[j++]);

#ifdef USE_MONOCHROME
  m->mono.a = f[j++];
  m->mono.b = f[j++];
  m->mono.size = f[j++];
  m->mono.highlights = f[j++];
#endif

  return j;
}

static inline void write_hex(FILE *f, uint8_t *input, int len)
{
  const char hex[16] =
  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8',
    '9', 'a', 'b', 'c', 'd', 'e', 'f'
  };

  for(int i=0; i<len; i++)
  {
    const int hi = input[i] >> 4;
    const int lo = input[i] & 15;
    fputc(hex[hi], f);
    fputc(hex[lo], f);
  }
}

static inline void write_xmp(module_params_t *m)
{
  FILE *f = fopen("input.xmp", "wb");
#ifdef USE_MONOCHROME
  fwrite(template_head_xmp, template_head_xmp_len, 1, f);
#else
  fwrite(template_color_head_xmp, template_color_head_xmp_len, 1, f);
#endif

  // write module params
  fprintf(f, "<rdf:li>");
  write_hex(f, (uint8_t *)&m->exp, sizeof(dt_iop_exposure_params_t));
  fprintf(f, "</rdf:li>\n");
  fprintf(f, "<rdf:li>");
  write_hex(f, (uint8_t *)&m->corr, sizeof(dt_iop_colorcorrection_params_t));
  fprintf(f, "</rdf:li>\n");
  fprintf(f, "<rdf:li>");
  write_hex(f, (uint8_t *)&m->curve, sizeof(dt_iop_tonecurve_params_t));
  fprintf(f, "</rdf:li>\n");
  fprintf(f, "<rdf:li>");
  write_hex(f, (uint8_t *)&m->zones, sizeof(dt_iop_colorzones_params_t));
  fprintf(f, "</rdf:li>\n");
  fprintf(f, "<rdf:li>");
  write_hex(f, (uint8_t *)&m->mono, sizeof(dt_iop_monochrome_params_t));
  fprintf(f, "</rdf:li>\n");

  fwrite(template_foot_xmp, template_foot_xmp_len, 1, f);
  fclose(f);
}

typedef struct opt_data_t
{
  module_params_t *m;
}
opt_data_t;


void eval_diff(float *param, float *sample, int param_cnt, int sample_cnt, void *data)
{
  // now the nasty part.
  opt_data_t *d = (opt_data_t *)data;
  int check = float2params(param, d->m);
  assert(check == param_cnt);
  write_xmp(d->m);
  // execute dt-cli
  system("rm output.pfm input.pfm.xmp");
  system("darktable-cli input.pfm input.xmp output.pfm");
  // read back image and write to float *sample
  FILE *f = fopen("output.pfm", "rb");
  fscanf(f, "PF\n%*d %*d\n%*[^\n]");
  fgetc(f); // \n
  fread(sample, sizeof(float), sample_cnt, f);
  fclose(f);
}

int main(int argc, char *argv[])
{
  // get initial data
  opt_data_t data;
  data.m = init_params();

  float *param = (float *)malloc(sizeof(float)*100);
  const int param_cnt = params2float(data.m, param);
  assert(param_cnt <= 100);

  // load reference output image into sample array:
  FILE *f = fopen("reference.pfm", "rb");
  if(!f)
  {
    fprintf(stderr, "usage: put into this directory: input.pfm, reference.pfm; then run.\n");
    exit(1);
  }
  int width, height;
  fscanf(f, "PF\n%d %d\n%*[^\n]", &width, &height);
  fgetc(f); // \n
  const int sample_cnt = 3*width*height;
  float *sample = (float *)malloc(sizeof(float)*sample_cnt);
  fread(sample, sizeof(float), sample_cnt, f);
  fclose(f);

  fprintf(stdout, "[fit] optimizing %d params over %d samples.\n", param_cnt, sample_cnt);

  float opts[LM_OPTS_SZ], info[LM_INFO_SZ];
  // opts[0]=LM_INIT_MU; opts[1]=1E-3; opts[2]=1E-5; opts[3]=1E-7; // terminates way to early
  // opts[0]=LM_INIT_MU; opts[1]=1E-7; opts[2]=1E-7; opts[3]=1E-12; // known to go through
  // opts[0]=LM_INIT_MU; opts[1]=1E-7; opts[2]=1E-7; opts[3]=1E-12; // known to go through
  // opts[0]=LM_INIT_MU; opts[1]=1E-7; opts[2]=1E-8; opts[3]=1E-13; // goes through, some nans
  opts[0]=LM_INIT_MU; opts[1]=1E-8; opts[2]=1E-8; opts[3]=1E-15;
  opts[4]= LM_DIFF_DELTA;
  slevmar_dif(eval_diff, param, sample, param_cnt, sample_cnt, 1000, opts, info, NULL, NULL, &data);

  // store final parameters:
  write_xmp(data.m);

  free(data.m);
  free(param);
  free(sample);
  exit(0);
}

