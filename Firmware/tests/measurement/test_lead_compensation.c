/* Independent double-precision fixture oracle; synthetic, not physical accuracy. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "measurement/measurement_lead_compensation.h"
typedef struct { double re, im; } dc_t;
static dc_t add(dc_t a, dc_t b) { return (dc_t){a.re+b.re,a.im+b.im}; }
static dc_t sub(dc_t a, dc_t b) { return (dc_t){a.re-b.re,a.im-b.im}; }
static dc_t mul(dc_t a, dc_t b) { return (dc_t){a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re}; }
static dc_t divc(dc_t a, dc_t b)
{
    const double d=b.re*b.re+b.im*b.im;
    return (dc_t){(a.re*b.re+a.im*b.im)/d,(a.im*b.re-a.re*b.im)/d};
}
static dc_t from(measurement_complex_t a) { return (dc_t){a.re,a.im}; }
static measurement_complex_t fp(dc_t a) { return measurement_complex((float)a.re,(float)a.im); }
static dc_t ratio(dc_t z, const measurement_cal_osl_coefficients_t *factory)
{
    return divc(sub(mul(z,from(factory->t_open)),mul(from(factory->k),from(factory->t_short))),sub(z,from(factory->k)));
}
static measurement_cal_osl_coefficients_t factory_for(float rref)
{
    return (measurement_cal_osl_coefficients_t){.k={-rref,rref*0.001f},.t_short={0.001f,-0.0002f},.t_open={0.985f,0.003f}};
}
int main(void)
{
    const float references[]={10.0f,100.0f,1000.0f,10000.0f,100000.0f,1000000.0f};
    const unsigned frequencies[]={10u,1000u,10000u};
    unsigned cases=0u;
    double maximum=0.0;
    for (unsigned f=0u; f<3u; ++f)
    {
        const double omega=6.283185307179586*(double)frequencies[f];
        const dc_t zs={0.42,omega*0.7e-6}, yo={2e-10,omega*80e-12};
        const measurement_lead_frequency_t lead={.series_ohms=fp(zs),.shunt_siemens=fp(yo)};
        for (unsigned r=0u; r<6u; ++r)
        {
            const double ref=references[r];
            const measurement_cal_osl_coefficients_t factory=factory_for(references[r]);
            const dc_t duts[]={{ref*0.1,0.0},{ref,0.0},{ref*10.0,0.0},{ref*0.02,ref},{ref*0.02,-ref},{ref*0.1,ref*10.0}};
            for (unsigned c=0u; c<6u; ++c)
            {
                const dc_t fixture=add(zs,divc(duts[c],add((dc_t){1.0,0.0},mul(yo,duts[c]))));
                measurement_complex_t out;
                assert(measurement_lead_apply_ratio(&factory,&lead,fp(ratio(fixture,&factory)),&out)==MEASUREMENT_STATUS_OK);
                const double error=hypot((double)out.re-duts[c].re,(double)out.im-duts[c].im)/hypot(duts[c].re,duts[c].im);
                if (error>maximum) maximum=error;
                assert(error<0.002); /* Float/model conditioning tolerance, not a metrology specification. */
                ++cases;
            }
        }
        const measurement_cal_osl_coefficients_t short_factory=factory_for(10.0f), open_factory=factory_for(1000000.0f);
        measurement_complex_t series, shunt;
        assert(measurement_lead_series(&short_factory,fp(ratio(zs,&short_factory)),&series)==MEASUREMENT_STATUS_OK);
        assert(hypot((double)series.re-zs.re,(double)series.im-zs.im)<0.0001);
        const dc_t zopen=add(zs,divc((dc_t){1.0,0.0},yo));
        assert(measurement_lead_shunt(&open_factory,fp(ratio(zopen,&open_factory)),series,&shunt)==MEASUREMENT_STATUS_OK);
        assert(hypot((double)shunt.re-yo.re,(double)shunt.im-yo.im)/hypot(yo.re,yo.im)<0.002);
    }
    const measurement_cal_osl_coefficients_t ideal={.k={-1000.0f,0.0f},.t_short={0.0f,0.0f},.t_open={1.0f,0.0f}};
    measurement_lead_frequency_t lead={.series_ohms={0.5f,0.0f},.shunt_siemens={0.0f,1e-6f}};
    measurement_complex_t out;
    assert(measurement_lead_apply_ratio(&ideal,&lead,ideal.t_open,&out)==MEASUREMENT_STATUS_OK);
    assert(fabsf(out.im-1000000.0f)<200.0f); /* Finite corrected DUT across the factory OPEN pole. */
    lead=(measurement_lead_frequency_t){0};
    assert(measurement_lead_apply_ratio(&ideal,&lead,measurement_complex(0.5f,0.0f),&out)==MEASUREMENT_STATUS_OK);
    assert(fabsf(out.re-1000.0f)<0.01f);
    assert(measurement_lead_apply_ratio(&ideal,&lead,ideal.t_open,&out)==MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL);
    assert(measurement_lead_shunt(&ideal,ideal.t_open,measurement_complex(0.0f,0.0f),&out)==MEASUREMENT_STATUS_OK);
    assert(out.re==0.0f && out.im==0.0f);
    lead.series_ohms=measurement_complex(-0.2f,-0.05f);
    assert(measurement_lead_frequency_valid(&lead,1000u)); /* A shorter fixture is a signed delta. */
    lead.series_ohms.re=NAN;
    assert(!measurement_lead_frequency_valid(&lead,1000u));
    measurement_lead_profile_t profile={.hardware_revision=MEASUREMENT_CAL_HARDWARE_REV1,
        .model_version=MEASUREMENT_CAL_MODEL_VERSION_CURRENT,.factory_sequence=1u,.factory_crc32=42u,.coverage=7u,.enabled=true};
    profile.uid[0]=1u;
    uint8_t uid[12]; memcpy(uid,profile.uid,12u);
    assert(measurement_lead_profile_bound(&profile,uid,1u,42u));
    assert(!measurement_lead_profile_bound(&profile,uid,1u,43u));
    profile.cleared=true;
    assert(!measurement_lead_profile_valid(&profile));
    printf("%u synthetic float cases PASS; max relative algebra error %.9g (not physical accuracy)\n",cases,maximum);
    return 0;
}
