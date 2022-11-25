/**
 *
 *  Copyright 2016-2020 Netflix, Inc.
 *
 *     Licensed under the BSD+Patent License (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://opensource.org/licenses/BSDplusPatent
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "cpu.h"
#include "mem.h"
#include "common/convolution.h"
#include "vif_options.h"
#include "vif_tools.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#ifdef VIF_OPT_FAST_LOG2 // option to replace log2 calculation with faster speed

static const float log2_poly_s[9] = { -0.012671635276421, 0.064841182402670, -0.157048836463065, 0.257167726303123, -0.353800560300520, 0.480131410397451, -0.721314327952201, 1.442694803896991, 0 };

static float horner_s(const float *poly, float x, int n)
{
    float var = 0;
    int i;

    for (i = 0; i < n; ++i) {
        var = var * x + poly[i];
    }
    return var;
}

static float log2f_approx(float x)
{
    const uint32_t exp_zero_const = 0x3F800000UL;

    const uint32_t exp_expo_mask = 0x7F800000UL;
    const uint32_t exp_mant_mask = 0x007FFFFFUL;

    float remain;
    float log_base, log_remain;
    uint32_t u32, u32remain;
    uint32_t exponent, mant;

    if (x == 0)
        return -INFINITY;
    if (x < 0)
        return NAN;

    memcpy(&u32, &x, sizeof(float));

    exponent  = (u32 & exp_expo_mask) >> 23;
    mant      = (u32 & exp_mant_mask) >> 0;
    u32remain = mant | exp_zero_const;

    memcpy(&remain, &u32remain, sizeof(float));

    log_base = (int32_t)exponent - 127;
    log_remain = horner_s(log2_poly_s, (remain - 1.0f), sizeof(log2_poly_s) / sizeof(float));

    return log_base + log_remain;
}

#define log2f log2f_approx

#endif /* VIF_FAST_LOG2 */

#if 1 //defined(_MSC_VER)
const float vif_filter1d_table_s[11][4][65] = {
        { // kernelscale = 1.0
                {0.00745626912, 0.0142655009, 0.0250313189, 0.0402820669, 0.0594526194, 0.0804751068, 0.0999041125, 0.113746084, 0.118773937, 0.113746084, 0.0999041125, 0.0804751068, 0.0594526194, 0.0402820669, 0.0250313189, 0.0142655009, 0.00745626912},
                {0.0189780835, 0.0558981746, 0.120920904, 0.192116052, 0.224173605, 0.192116052, 0.120920904, 0.0558981746, 0.0189780835},
                {0.054488685, 0.244201347, 0.402619958, 0.244201347, 0.054488685},
                {0.166378498, 0.667243004, 0.166378498}
        },
        { // kernelscale = 0.5
                {0.01483945381483146, 0.04981728920101665, 0.11832250618647212, 0.19882899654808195, 0.2363835084991954, 0.19882899654808195, 0.11832250618647212, 0.04981728920101665, 0.01483945381483146},
                {0.03765705331865383, 0.23993597758503457, 0.4448139381926232, 0.23993597758503457, 0.03765705331865383},
                {0.10650697891920077, 0.7869860421615985, 0.10650697891920077},
                {0.00383625879916893, 0.9923274824016621, 0.00383625879916893}
        },
        { // kernelscale = 1.5
                {0.003061411879755733, 0.004950361473107714, 0.007702910451476288, 0.011533883865671363, 0.01661877803810386, 0.02304227676257057, 0.030743582802885815, 0.039471747635457924, 0.04876643642871142, 0.057977365173867444, 0.06632827707341536, 0.07301998612791924, 0.0773548527900372, 0.07885625899404015, 0.0773548527900372, 0.07301998612791924, 0.06632827707341536, 0.057977365173867444, 0.04876643642871142, 0.039471747635457924, 0.030743582802885815, 0.02304227676257057, 0.01661877803810386, 0.011533883865671363, 0.007702910451476288, 0.004950361473107714, 0.003061411879755733},
                {0.005155279152239917, 0.012574282300781493, 0.026738695950137843, 0.049570492793231676, 0.08011839616706516, 0.11289306247174812, 0.13868460659463525, 0.14853036914032117, 0.13868460659463525, 0.11289306247174812, 0.08011839616706516, 0.049570492793231676, 0.026738695950137843, 0.012574282300781493, 0.005155279152239917},
                {0.007614419169296345, 0.03607496968918391, 0.10958608179781394, 0.2134445419434044, 0.26655997480060273, 0.2134445419434044, 0.10958608179781394, 0.03607496968918391, 0.007614419169296345},
                {0.03765705331865383, 0.23993597758503457, 0.4448139381926232, 0.23993597758503457, 0.03765705331865383}
        },
        { // kernelscale = 2.0
                {0.0026037269587503567, 0.0037202012799425984, 0.005201699435890725, 0.007117572324831842, 0.009530733850673961, 0.012489027248675568, 0.016015433656466918, 0.02009817440675194, 0.024682113028122028, 0.02966305528852977, 0.03488648739727233, 0.04015192995666896, 0.045223422276224175, 0.04984576229541491, 0.05376515201297315, 0.05675202106130506, 0.058623211304833, 0.059260552433345506, 0.058623211304833, 0.05675202106130506, 0.05376515201297315, 0.04984576229541491, 0.045223422276224175, 0.04015192995666896, 0.03488648739727233, 0.02966305528852977, 0.024682113028122028, 0.02009817440675194, 0.016015433656466918, 0.012489027248675568, 0.009530733850673961, 0.007117572324831842, 0.005201699435890725, 0.0037202012799425984, 0.0026037269587503567},
                {0.004908790159284653, 0.009458290945313327, 0.01687098713840658, 0.027858513730912443, 0.04258582005051434, 0.06026452109898014, 0.07894925354042928, 0.09574673424578385, 0.10749531455964786, 0.11172354906145505, 0.10749531455964786, 0.09574673424578385, 0.07894925354042928, 0.06026452109898014, 0.04258582005051434, 0.027858513730912443, 0.01687098713840658, 0.009458290945313327, 0.004908790159284653},
                {0.008812229292562283, 0.027143577143479366, 0.06511405659938266, 0.12164907301380957, 0.17699835683135567, 0.2005654142388208, 0.17699835683135567, 0.12164907301380957, 0.06511405659938266, 0.027143577143479366, 0.008812229292562283},
                {0.014646255580395366, 0.08312087071417153, 0.23555925344404363, 0.3333472405227789, 0.23555925344404363, 0.08312087071417153, 0.014646255580395366}
        },
        { // kernelscale = 2.0/3
                {0.005316919191158936, 0.015508616400966175, 0.03723543338250509, 0.07358853152438767, 0.11971104580936091, 0.16029821187968224, 0.17668248362387792, 0.16029821187968224, 0.11971104580936091, 0.07358853152438767, 0.03723543338250509, 0.015508616400966175, 0.005316919191158936},
                {0.014646255580395366, 0.08312087071417153, 0.23555925344404363, 0.3333472405227789, 0.23555925344404363, 0.08312087071417153, 0.014646255580395366},
                {0.006646032999923536, 0.1942255544092176, 0.5982568251817177, 0.1942255544092176, 0.006646032999923536},
                {0.04038789325328935, 0.9192242134934214, 0.04038789325328935}
        },
        { // kernelscale = 2.4
                {0.0024545290205999935, 0.003289682353098358, 0.004343275824783345, 0.005648830035496486, 0.007237311348814264, 0.009134266042662214, 0.01135658376291313, 0.0139091122480005, 0.01678142232148741, 0.019945079588083118, 0.023351803735371327, 0.026932876628807743, 0.030600089953844747, 0.0342484022907008, 0.03776031258766088, 0.04101176848166929, 0.04387923676578804, 0.046247395983335465, 0.048016793525188006, 0.04911076259025178, 0.04948092982288614, 0.04911076259025178, 0.048016793525188006, 0.046247395983335465, 0.04387923676578804, 0.04101176848166929, 0.03776031258766088, 0.0342484022907008, 0.030600089953844747, 0.026932876628807743, 0.023351803735371327, 0.019945079588083118, 0.01678142232148741, 0.0139091122480005, 0.01135658376291313, 0.009134266042662214, 0.007237311348814264, 0.005648830035496486, 0.004343275824783345, 0.003289682353098358, 0.0024545290205999935},
                {0.003637908247873508, 0.0063855489460743495, 0.010623647203380349, 0.016752435202957758, 0.02503866437045789, 0.03547098721241576, 0.047628214164962705, 0.0606155744031302, 0.07311947377207272, 0.08360086952227502, 0.09059775517499266, 0.09305784355881423, 0.09059775517499266, 0.08360086952227502, 0.07311947377207272, 0.0606155744031302, 0.047628214164962705, 0.03547098721241576, 0.02503866437045789, 0.016752435202957758, 0.010623647203380349, 0.0063855489460743495, 0.003637908247873508},
                {0.007350293016136075, 0.019098337505782634, 0.04171460426536077, 0.07659181203740582, 0.11821653158869994, 0.15338247260964522, 0.1672918979539392, 0.15338247260964522, 0.11821653158869994, 0.07659181203740582, 0.04171460426536077, 0.019098337505782634, 0.007350293016136075},
                {0.00585668412070982, 0.03167315245079435, 0.10575256915549137, 0.21799709606017553, 0.27744099642565795, 0.21799709606017553, 0.10575256915549137, 0.03167315245079435, 0.00585668412070982}
        },
        { // kernelscale = 360.0 / 97.0 (1080p -> 291p)
                {0.0012816791204082075, 0.0015620523909898277, 0.0018918399027484485, 0.0022769089575337843, 0.0027231994378439997, 0.003236575413413638, 0.0038226497837505957, 0.004486583874601567, 0.005232865467640872, 0.006065070407937307, 0.006985614620544574, 0.00799550497717726, 0.009094098875819743, 0.010278883514029925, 0.011545286536358305, 0.012886529914069893, 0.01429353848739285, 0.01575491351167377, 0.017256979780457923, 0.018783912474459312, 0.02031794687526095, 0.021839670601917226, 0.023328394235385276, 0.024762592283158524, 0.026120402622622926, 0.02738016907584346, 0.028521008836012274, 0.02952338429169555, 0.03036965754835163, 0.03104460574645926, 0.03153587618024757, 0.03183436222109175, 0.03193448406620358, 0.03183436222109175, 0.03153587618024757, 0.03104460574645926, 0.03036965754835163, 0.02952338429169555, 0.028521008836012274, 0.02738016907584346, 0.026120402622622926, 0.024762592283158524, 0.023328394235385276, 0.021839670601917226, 0.02031794687526095, 0.018783912474459312, 0.017256979780457923, 0.01575491351167377, 0.01429353848739285, 0.012886529914069893, 0.011545286536358305, 0.010278883514029925, 0.009094098875819743, 0.00799550497717726, 0.006985614620544574, 0.006065070407937307, 0.005232865467640872, 0.004486583874601567, 0.0038226497837505957, 0.003236575413413638, 0.0027231994378439997, 0.0022769089575337843, 0.0018918399027484485, 0.0015620523909898277, 0.0012816791204082075},
                {0.0023644174536255184, 0.0034221036550523224, 0.0048431811011422285, 0.006702499696259348, 0.009070086779378358, 0.012002027755765152, 0.015529817658257908, 0.01964927955626293, 0.02431058735737885, 0.029411204931348196, 0.03479354885462776, 0.04024882291916349, 0.0455277497133199, 0.05035791396527277, 0.0544662936498729, 0.05760450646430479, 0.05957357217624005, 0.060244772625455, 0.05957357217624005, 0.05760450646430479, 0.0544662936498729, 0.05035791396527277, 0.0455277497133199, 0.04024882291916349, 0.03479354885462776, 0.029411204931348196, 0.02431058735737885, 0.01964927955626293, 0.015529817658257908, 0.012002027755765152, 0.009070086779378358, 0.006702499696259348, 0.0048431811011422285, 0.0034221036550523224, 0.0023644174536255184},
                {0.005739705984167229, 0.010638831007972463, 0.018338687970180532, 0.029397655644830867, 0.04382553457697562, 0.06075917001712261, 0.07833692675250763, 0.09392718641207363, 0.10473363656553764, 0.10860533013726342, 0.10473363656553764, 0.09392718641207363, 0.07833692675250763, 0.06075917001712261, 0.04382553457697562, 0.029397655644830867, 0.018338687970180532, 0.010638831007972463, 0.005739705984167229},
                {0.004765886490117418, 0.014449429659230515, 0.03580755131873149, 0.07252962766809999, 0.1200806914901692, 0.16249792490873866, 0.17973777692982543, 0.16249792490873866, 0.1200806914901692, 0.07252962766809999, 0.03580755131873149, 0.014449429659230515, 0.004765886490117418}
        },
        { // kernelscale = 4.0 / 3.0
                {0.00468593450369258, 0.007810637942127317, 0.012400648305837857, 0.018752961660127826, 0.027012385060515166, 0.0370615509191398, 0.048434167336838044, 0.06029033231050093, 0.07148437238577607, 0.0807313343364795, 0.08684418500467246, 0.08898298046858494, 0.08684418500467246, 0.0807313343364795, 0.07148437238577607, 0.06029033231050093, 0.048434167336838044, 0.0370615509191398, 0.027012385060515166, 0.018752961660127826, 0.012400648305837857, 0.007810637942127317, 0.00468593450369258},
                {0.007350293016136075, 0.019098337505782634, 0.04171460426536077, 0.07659181203740582, 0.11821653158869994, 0.15338247260964522, 0.1672918979539392, 0.15338247260964522, 0.11821653158869994, 0.07659181203740582, 0.04171460426536077, 0.019098337505782634, 0.007350293016136075},
                {0.023977406661157635, 0.09784278911234541, 0.22749130044200694, 0.30137700756898, 0.22749130044200694, 0.09784278911234541, 0.023977406661157635},
                {0.021929644862389363, 0.22851214688447105, 0.49911641650627925, 0.22851214688447105, 0.021929644862389363}
        },
        { // kernelscale = 3.5 / 3.0
                {0.004225481445163885, 0.0077284175478382604, 0.013264883597653383, 0.021365585324475772, 0.03229420764197194, 0.04580711688347506, 0.060973309616983565, 0.07616317964123727, 0.08927890406929252, 0.09820896199705818, 0.10137990446970037, 0.09820896199705818, 0.08927890406929252, 0.07616317964123727, 0.060973309616983565, 0.04580711688347506, 0.03229420764197194, 0.021365585324475772, 0.013264883597653383, 0.0077284175478382604, 0.004225481445163885},
                {0.011253064073270563, 0.03121967849226136, 0.06904092264126249, 0.12170411773975143, 0.17101117222357906, 0.19154208965975011, 0.17101117222357906, 0.12170411773975143, 0.06904092264126249, 0.03121967849226136, 0.011253064073270563},
                {0.012560200468474614, 0.07882796468173003, 0.23729607711717063, 0.34263151546524945, 0.23729607711717063, 0.07882796468173003, 0.012560200468474614},
                {0.009620056834605945, 0.20542369732245147, 0.5699124916858852, 0.20542369732245147, 0.009620056834605945}
        },
        { // kernelscale = 3.75 / 3.0
                {0.003317211135862137, 0.0059324619144260895, 0.01003813004663814, 0.01607040018215851, 0.024342018059788077, 0.03488530174522498, 0.04730253352513053, 0.06068513681455791, 0.07366077493233217, 0.08459530222840861, 0.09192046741565756, 0.09450052399963076, 0.09192046741565756, 0.08459530222840861, 0.07366077493233217, 0.06068513681455791, 0.04730253352513053, 0.03488530174522498, 0.024342018059788077, 0.01607040018215851, 0.01003813004663814, 0.0059324619144260895, 0.003317211135862137},
                {0.0050830940167887065, 0.01506448350708729, 0.03664323313832898, 0.0731554625933815, 0.11987073595060249, 0.16121038612766267, 0.17794520933229682, 0.16121038612766267, 0.11987073595060249, 0.0731554625933815, 0.03664323313832898, 0.01506448350708729, 0.0050830940167887065},
                {0.017988208587689274, 0.08909618039160763, 0.2326921801242316, 0.3204468617929429, 0.2326921801242316, 0.08909618039160763, 0.017988208587689274},
                {0.015199625365883403, 0.21875173294350592, 0.5320972833812214, 0.21875173294350592, 0.015199625365883403}
        },
        { // kernelscale = 4.25 / 3.0
                {0.0037535214972137234, 0.006161856952968067, 0.009688687555946651, 0.014591465886006169, 0.021048130695628536, 0.02908096232945107, 0.03848439382348116, 0.04877992869222271, 0.059221350419380495, 0.06886460899713377, 0.07669984741736877, 0.08182265140547183, 0.08360518865545416, 0.08182265140547183, 0.07669984741736877, 0.06886460899713377, 0.059221350419380495, 0.04877992869222271, 0.03848439382348116, 0.02908096232945107, 0.021048130695628536, 0.014591465886006169, 0.009688687555946651, 0.006161856952968067, 0.0037535214972137234},
                {0.009923596815614544, 0.023121061729092417, 0.0461910237358901, 0.07912588025538068, 0.11622262324531331, 0.14637737178419374, 0.15807688486903043, 0.14637737178419374, 0.11622262324531331, 0.07912588025538068, 0.0461910237358901, 0.023121061729092417, 0.009923596815614544},
                {0.005235891862728509, 0.029948577627467506, 0.10407966050653025, 0.21976557644809397, 0.2819405871103595, 0.21976557644809397, 0.10407966050653025, 0.029948577627467506, 0.005235891862728509},
                {0.029519066107809168, 0.23537051469301665, 0.47022083839834844, 0.23537051469301665, 0.029519066107809168}
        }
};
#else
const float vif_filter1d_table_s[11][4][65] = {
        { // kernelscale = 1.0
                { 0x1.e8a77p-8,  0x1.d373b2p-7, 0x1.9a1cf6p-6, 0x1.49fd9ep-5, 0x1.e7092ep-5, 0x1.49a044p-4, 0x1.99350ep-4, 0x1.d1e76ap-4, 0x1.e67f8p-4, 0x1.d1e76ap-4, 0x1.99350ep-4, 0x1.49a044p-4, 0x1.e7092ep-5, 0x1.49fd9ep-5, 0x1.9a1cf6p-6, 0x1.d373b2p-7, 0x1.e8a77p-8 },
                { 0x1.36efdap-6, 0x1.c9eaf8p-5, 0x1.ef4ac2p-4, 0x1.897424p-3, 0x1.cb1b88p-3, 0x1.897424p-3, 0x1.ef4ac2p-4, 0x1.c9eaf8p-5, 0x1.36efdap-6 },
                { 0x1.be5f0ep-5, 0x1.f41fd6p-3, 0x1.9c4868p-2, 0x1.f41fd6p-3, 0x1.be5f0ep-5 },
                { 0x1.54be4p-3,  0x1.55a0ep-1,  0x1.54be4p-3 }
        },
        { // kernelscale = 0.5
                { 1.483945e-02, 4.981729e-02, 1.183225e-01, 1.988290e-01, 2.363835e-01, 1.988290e-01, 1.183225e-01, 4.981729e-02, 1.483945e-02 },
                { 3.765705e-02, 2.399360e-01, 4.448139e-01, 2.399360e-01, 3.765705e-02 },
                { 1.065070e-01, 7.869860e-01, 1.065070e-01 },
                { 3.836259e-03, 9.923275e-01, 3.836259e-03 }
        },
        { // kernelscale = 1.5
                { 3.061412e-03, 4.950361e-03, 7.702910e-03, 1.153388e-02, 1.661878e-02, 2.304228e-02, 3.074358e-02, 3.947175e-02, 4.876644e-02, 5.797737e-02, 6.632828e-02, 7.301999e-02, 7.735485e-02, 7.885626e-02, 7.735485e-02, 7.301999e-02, 6.632828e-02, 5.797737e-02, 4.876644e-02, 3.947175e-02, 3.074358e-02, 2.304228e-02, 1.661878e-02, 1.153388e-02, 7.702910e-03, 4.950361e-03, 3.061412e-03 },
                { 5.155279e-03, 1.257428e-02, 2.673870e-02, 4.957049e-02, 8.011840e-02, 1.128931e-01, 1.386846e-01, 1.485304e-01, 1.386846e-01, 1.128931e-01, 8.011840e-02, 4.957049e-02, 2.673870e-02, 1.257428e-02, 5.155279e-03 },
                { 7.614419e-03, 3.607497e-02, 1.095861e-01, 2.134445e-01, 2.665600e-01, 2.134445e-01, 1.095861e-01, 3.607497e-02, 7.614419e-03 },
                { 3.765705e-02, 2.399360e-01, 4.448139e-01, 2.399360e-01, 3.765705e-02 }
        },
        { // kernelscale = 2.0
                { 2.603727e-03, 3.720201e-03, 5.201699e-03, 7.117572e-03, 9.530734e-03, 1.248903e-02, 1.601543e-02, 2.009817e-02, 2.468211e-02, 2.966306e-02, 3.488649e-02, 4.015193e-02, 4.522342e-02, 4.984576e-02, 5.376515e-02, 5.675202e-02, 5.862321e-02, 5.926055e-02, 5.862321e-02, 5.675202e-02, 5.376515e-02, 4.984576e-02, 4.522342e-02, 4.015193e-02, 3.488649e-02, 2.966306e-02, 2.468211e-02, 2.009817e-02, 1.601543e-02, 1.248903e-02, 9.530734e-03, 7.117572e-03, 5.201699e-03, 3.720201e-03, 2.603727e-03 },
                { 4.908790e-03, 9.458291e-03, 1.687099e-02, 2.785851e-02, 4.258582e-02, 6.026452e-02, 7.894925e-02, 9.574673e-02, 1.074953e-01, 1.117235e-01, 1.074953e-01, 9.574673e-02, 7.894925e-02, 6.026452e-02, 4.258582e-02, 2.785851e-02, 1.687099e-02, 9.458291e-03, 4.908790e-03 },
                { 8.812229e-03, 2.714358e-02, 6.511406e-02, 1.216491e-01, 1.769984e-01, 2.005654e-01, 1.769984e-01, 1.216491e-01, 6.511406e-02, 2.714358e-02, 8.812229e-03 },
                { 1.464626e-02, 8.312087e-02, 2.355593e-01, 3.333472e-01, 2.355593e-01, 8.312087e-02, 1.464626e-02 }
        },
        { // kernelscale = 2.0/3
                { 5.316919e-03, 1.550862e-02, 3.723543e-02, 7.358853e-02, 1.197110e-01, 1.602982e-01, 1.766825e-01, 1.602982e-01, 1.197110e-01, 7.358853e-02, 3.723543e-02, 1.550862e-02, 5.316919e-03 },
                { 1.464626e-02, 8.312087e-02, 2.355593e-01, 3.333472e-01, 2.355593e-01, 8.312087e-02, 1.464626e-02 },
                { 6.646033e-03, 1.942256e-01, 5.982568e-01, 1.942256e-01, 6.646033e-03 },
                { 4.038789e-02, 9.192242e-01, 4.038789e-02 }
        },
        { // kernelscale = 2.4
                { 2.454529e-03, 3.289682e-03, 4.343276e-03, 5.648830e-03, 7.237311e-03, 9.134266e-03, 1.135658e-02, 1.390911e-02, 1.678142e-02, 1.994508e-02, 2.335180e-02, 2.693288e-02, 3.060009e-02, 3.424840e-02, 3.776031e-02, 4.101177e-02, 4.387924e-02, 4.624740e-02, 4.801679e-02, 4.911076e-02, 4.948093e-02, 4.911076e-02, 4.801679e-02, 4.624740e-02, 4.387924e-02, 4.101177e-02, 3.776031e-02, 3.424840e-02, 3.060009e-02, 2.693288e-02, 2.335180e-02, 1.994508e-02, 1.678142e-02, 1.390911e-02, 1.135658e-02, 9.134266e-03, 7.237311e-03, 5.648830e-03, 4.343276e-03, 3.289682e-03, 2.454529e-03 },
                { 3.637908e-03, 6.385549e-03, 1.062365e-02, 1.675244e-02, 2.503866e-02, 3.547099e-02, 4.762821e-02, 6.061557e-02, 7.311947e-02, 8.360087e-02, 9.059776e-02, 9.305784e-02, 9.059776e-02, 8.360087e-02, 7.311947e-02, 6.061557e-02, 4.762821e-02, 3.547099e-02, 2.503866e-02, 1.675244e-02, 1.062365e-02, 6.385549e-03, 3.637908e-03 },
                { 7.350293e-03, 1.909834e-02, 4.171460e-02, 7.659181e-02, 1.182165e-01, 1.533825e-01, 1.672919e-01, 1.533825e-01, 1.182165e-01, 7.659181e-02, 4.171460e-02, 1.909834e-02, 7.350293e-03 },
                { 5.856684e-03, 3.167315e-02, 1.057526e-01, 2.179971e-01, 2.774410e-01, 2.179971e-01, 1.057526e-01, 3.167315e-02, 5.856684e-03 }
        },
        { // kernelscale = 360.0 / 97.0 (1080p -> 291p)
                { 1.281679e-03, 1.562052e-03, 1.891840e-03, 2.276909e-03, 2.723199e-03, 3.236575e-03, 3.822650e-03, 4.486584e-03, 5.232865e-03, 6.065070e-03, 6.985615e-03, 7.995505e-03, 9.094099e-03, 1.027888e-02, 1.154529e-02, 1.288653e-02, 1.429354e-02, 1.575491e-02, 1.725698e-02, 1.878391e-02, 2.031795e-02, 2.183967e-02, 2.332839e-02, 2.476259e-02, 2.612040e-02, 2.738017e-02, 2.852101e-02, 2.952338e-02, 3.036966e-02, 3.104461e-02, 3.153588e-02, 3.183436e-02, 3.193448e-02, 3.183436e-02, 3.153588e-02, 3.104461e-02, 3.036966e-02, 2.952338e-02, 2.852101e-02, 2.738017e-02, 2.612040e-02, 2.476259e-02, 2.332839e-02, 2.183967e-02, 2.031795e-02, 1.878391e-02, 1.725698e-02, 1.575491e-02, 1.429354e-02, 1.288653e-02, 1.154529e-02, 1.027888e-02, 9.094099e-03, 7.995505e-03, 6.985615e-03, 6.065070e-03, 5.232865e-03, 4.486584e-03, 3.822650e-03, 3.236575e-03, 2.723199e-03, 2.276909e-03, 1.891840e-03, 1.562052e-03, 1.281679e-03 },
                { 2.364417e-03, 3.422104e-03, 4.843181e-03, 6.702500e-03, 9.070087e-03, 1.200203e-02, 1.552982e-02, 1.964928e-02, 2.431059e-02, 2.941120e-02, 3.479355e-02, 4.024882e-02, 4.552775e-02, 5.035791e-02, 5.446629e-02, 5.760451e-02, 5.957357e-02, 6.024477e-02, 5.957357e-02, 5.760451e-02, 5.446629e-02, 5.035791e-02, 4.552775e-02, 4.024882e-02, 3.479355e-02, 2.941120e-02, 2.431059e-02, 1.964928e-02, 1.552982e-02, 1.200203e-02, 9.070087e-03, 6.702500e-03, 4.843181e-03, 3.422104e-03, 2.364417e-03 },
                { 5.739706e-03, 1.063883e-02, 1.833869e-02, 2.939766e-02, 4.382553e-02, 6.075917e-02, 7.833693e-02, 9.392719e-02, 1.047336e-01, 1.086053e-01, 1.047336e-01, 9.392719e-02, 7.833693e-02, 6.075917e-02, 4.382553e-02, 2.939766e-02, 1.833869e-02, 1.063883e-02, 5.739706e-03 },
                { 4.765886e-03, 1.444943e-02, 3.580755e-02, 7.252963e-02, 1.200807e-01, 1.624979e-01, 1.797378e-01, 1.624979e-01, 1.200807e-01, 7.252963e-02, 3.580755e-02, 1.444943e-02, 4.765886e-03 }
        },
        { // kernelscale = 4.0 / 3.0
                { 4.685935e-03, 7.810638e-03, 1.240065e-02, 1.875296e-02, 2.701239e-02, 3.706155e-02, 4.843417e-02, 6.029033e-02, 7.148437e-02, 8.073133e-02, 8.684419e-02, 8.898298e-02, 8.684419e-02, 8.073133e-02, 7.148437e-02, 6.029033e-02, 4.843417e-02, 3.706155e-02, 2.701239e-02, 1.875296e-02, 1.240065e-02, 7.810638e-03, 4.685935e-03 },
                { 7.350293e-03, 1.909834e-02, 4.171460e-02, 7.659181e-02, 1.182165e-01, 1.533825e-01, 1.672919e-01, 1.533825e-01, 1.182165e-01, 7.659181e-02, 4.171460e-02, 1.909834e-02, 7.350293e-03 },
                { 2.397741e-02, 9.784279e-02, 2.274913e-01, 3.013770e-01, 2.274913e-01, 9.784279e-02, 2.397741e-02 },
                { 2.192964e-02, 2.285121e-01, 4.991164e-01, 2.285121e-01, 2.192964e-02 }
        },
        { // kernelscale = 3.5 / 3.0
                { 4.225481e-03, 7.728418e-03, 1.326488e-02, 2.136559e-02, 3.229421e-02, 4.580712e-02, 6.097331e-02, 7.616318e-02, 8.927890e-02, 9.820896e-02, 1.013799e-01, 9.820896e-02, 8.927890e-02, 7.616318e-02, 6.097331e-02, 4.580712e-02, 3.229421e-02, 2.136559e-02, 1.326488e-02, 7.728418e-03, 4.225481e-03 },
                { 1.125306e-02, 3.121968e-02, 6.904092e-02, 1.217041e-01, 1.710112e-01, 1.915421e-01, 1.710112e-01, 1.217041e-01, 6.904092e-02, 3.121968e-02, 1.125306e-02 },
                { 1.256020e-02, 7.882796e-02, 2.372961e-01, 3.426315e-01, 2.372961e-01, 7.882796e-02, 1.256020e-02 },
                { 9.620057e-03, 2.054237e-01, 5.699125e-01, 2.054237e-01, 9.620057e-03 }
        },
        { // kernelscale = 3.75 / 3.0
                { 3.317211e-03, 5.932462e-03, 1.003813e-02, 1.607040e-02, 2.434202e-02, 3.488530e-02, 4.730253e-02, 6.068514e-02, 7.366077e-02, 8.459530e-02, 9.192047e-02, 9.450052e-02, 9.192047e-02, 8.459530e-02, 7.366077e-02, 6.068514e-02, 4.730253e-02, 3.488530e-02, 2.434202e-02, 1.607040e-02, 1.003813e-02, 5.932462e-03, 3.317211e-03 },
                { 5.083094e-03, 1.506448e-02, 3.664323e-02, 7.315546e-02, 1.198707e-01, 1.612104e-01, 1.779452e-01, 1.612104e-01, 1.198707e-01, 7.315546e-02, 3.664323e-02, 1.506448e-02, 5.083094e-03 },
                { 1.798821e-02, 8.909618e-02, 2.326922e-01, 3.204469e-01, 2.326922e-01, 8.909618e-02, 1.798821e-02 },
                { 1.519963e-02, 2.187517e-01, 5.320973e-01, 2.187517e-01, 1.519963e-02 }
        },
        { // kernelscale = 4.25 / 3.0
                { 3.753521e-03, 6.161857e-03, 9.688688e-03, 1.459147e-02, 2.104813e-02, 2.908096e-02, 3.848439e-02, 4.877993e-02, 5.922135e-02, 6.886461e-02, 7.669985e-02, 8.182265e-02, 8.360519e-02, 8.182265e-02, 7.669985e-02, 6.886461e-02, 5.922135e-02, 4.877993e-02, 3.848439e-02, 2.908096e-02, 2.104813e-02, 1.459147e-02, 9.688688e-03, 6.161857e-03, 3.753521e-03 },
                { 9.923597e-03, 2.312106e-02, 4.619102e-02, 7.912588e-02, 1.162226e-01, 1.463774e-01, 1.580769e-01, 1.463774e-01, 1.162226e-01, 7.912588e-02, 4.619102e-02, 2.312106e-02, 9.923597e-03 },
                { 5.235892e-03, 2.994858e-02, 1.040797e-01, 2.197656e-01, 2.819406e-01, 2.197656e-01, 1.040797e-01, 2.994858e-02, 5.235892e-03 },
                { 2.951907e-02, 2.353705e-01, 4.702208e-01, 2.353705e-01, 2.951907e-02 }
        }
};
#endif

const int vif_filter1d_width[11][4] = {
        { 17, 9, 5, 3 }, // kernelscale = 1.0
        { 9, 5, 3, 3 }, // kernelscale = 0.5
        { 27, 15, 9, 5 },  // kernelscale = 1.5
        { 35, 19, 11, 7 },  // kernelscale = 2.0
        { 13, 7, 5, 3 },  // kernelscale = 2.0/3
        { 41, 23, 13, 9 }, // kernelscale = 2.4/1.0
        { 65, 35, 19, 13 },  // kernelscale = 360.0/97.0
        { 23, 13, 7, 5 },  // kernelscale = 4.0/3.0
        { 21, 11, 7, 5 },  // kernelscale = 3.5/3.0
        { 23, 13, 7, 5 },  // kernelscale = 3.75/3.0
        { 25, 13, 9, 5 }  // kernelscale = 4.25/3.0
};

void vif_dec2_s(const float *src, float *dst, int src_w, int src_h, int src_stride, int dst_stride)
{
    int src_px_stride = src_stride / sizeof(float); // src_stride is in bytes
    int dst_px_stride = dst_stride / sizeof(float);

    int i, j;

    // decimation by 2 in each direction (after gaussian blur? check)
    for (i = 0; i < src_h / 2; ++i) {
        for (j = 0; j < src_w / 2; ++j) {
            dst[i * dst_px_stride + j] = src[(i * 2) * src_px_stride + (j * 2)];
        }
    }
}

float vif_sum_s(const float *x, int w, int h, int stride)
{
    int px_stride = stride / sizeof(float);
    int i, j;

    float accum = 0;

    for (i = 0; i < h; ++i) {
        float accum_inner = 0;

        for (j = 0; j < w; ++j) {
            accum_inner += x[i * px_stride + j];
        } // having an inner accumulator help reduce numerical error (no accumulation of near-0 terms)

        accum += accum_inner;
    }

    return accum;
}

void vif_statistic_s(const float *mu1, const float *mu2, const float *xx_filt, const float *yy_filt, const float *xy_filt, float *num, float *den,
	int w, int h, int mu1_stride, int mu2_stride, int xx_filt_stride, int yy_filt_stride, int xy_filt_stride,
	double vif_enhn_gain_limit)
{
	static const float sigma_nsq = 2;
	static const float sigma_max_inv = 4.0 / (255.0*255.0);

	int mu1_px_stride = mu1_stride / sizeof(float);
	int mu2_px_stride = mu2_stride / sizeof(float);
	int xx_filt_px_stride = xx_filt_stride / sizeof(float);
	int yy_filt_px_stride = yy_filt_stride / sizeof(float);
	int xy_filt_px_stride = xy_filt_stride / sizeof(float);

	float mu1_sq_val, mu2_sq_val, mu1_mu2_val, xx_filt_val, yy_filt_val, xy_filt_val;
	float sigma1_sq, sigma2_sq, sigma12;
	float num_val, den_val;
	int i, j;

    /* ==== vif_stat_mode = 'matching_c' ==== */
    // float num_log_den, num_log_num;
    /* ==== vif_stat_mode = 'matching_matlab' ==== */
	float g, sv_sq, eps = 1.0e-10f;
	float vif_enhn_gain_limit_f = (float) vif_enhn_gain_limit;
    /* == end of vif_stat_mode = 'matching_matlab' == */

	float accum_num = 0.0f;
	float accum_den = 0.0f;

	for (i = 0; i < h; ++i) {
		float accum_inner_num = 0;
		float accum_inner_den = 0;
		for (j = 0; j < w; ++j) {
			float mu1_val = mu1[i * mu1_px_stride + j];
			float mu2_val = mu2[i * mu2_px_stride + j];
			mu1_sq_val = mu1_val * mu1_val; // same name as the Matlab code vifp_mscale.m
			mu2_sq_val = mu2_val * mu2_val;
			mu1_mu2_val = mu1_val * mu2_val; //mu1_mu2[i * mu1_mu2_px_stride + j];
			xx_filt_val = xx_filt[i * xx_filt_px_stride + j];
			yy_filt_val = yy_filt[i * yy_filt_px_stride + j];
			xy_filt_val = xy_filt[i * xy_filt_px_stride + j];

			sigma1_sq = xx_filt_val - mu1_sq_val;
			sigma2_sq = yy_filt_val - mu2_sq_val;
			sigma12 = xy_filt_val - mu1_mu2_val;

			/* ==== vif_stat_mode = 'matching_c' ==== */

            /* if (sigma1_sq < sigma_nsq) {
                num_val = 1.0 - sigma2_sq * sigma_max_inv;
                den_val = 1.0;
            }
            else {
                num_log_num = (sigma2_sq + sigma_nsq) * sigma1_sq;
                if (sigma12 < 0)
                {
                    num_val = 0.0;
                }
                else
                {
                    num_log_den = num_log_num - sigma12 * sigma12;
                    num_val = log2f(num_log_num / num_log_den);
                }
                den_val = log2f(1.0f + sigma1_sq / sigma_nsq);
            } */

            /* ==== vif_stat_mode = 'matching_matlab' ==== */

            sigma1_sq = MAX(sigma1_sq, 0.0f);
            sigma2_sq = MAX(sigma2_sq, 0.0f);

			g = sigma12 / (sigma1_sq + eps);
			sv_sq = sigma2_sq - g * sigma12;

			if (sigma1_sq < eps) {
			    g = 0.0f;
                sv_sq = sigma2_sq;
                sigma1_sq = 0.0f;
			}

			if (sigma2_sq < eps) {
			    g = 0.0f;
			    sv_sq = 0.0f;
			}

			if (g < 0.0f) {
			    sv_sq = sigma2_sq;
			    g = 0.0f;
			}
			sv_sq = MAX(sv_sq, eps);

            g = MIN(g, vif_enhn_gain_limit_f);

            num_val = log2f(1.0f + (g * g * sigma1_sq) / (sv_sq + sigma_nsq));
            den_val = log2f(1.0f + (sigma1_sq) / (sigma_nsq));

            if (sigma12 < 0.0f) {
                num_val = 0.0f;
            }

            if (sigma1_sq < sigma_nsq) {
                num_val = 1.0f - sigma2_sq * sigma_max_inv;
                den_val = 1.0f;
            }

            /* == end of vif_stat_mode = 'matching_matlab' == */

            accum_inner_num += num_val;
			accum_inner_den += den_val;
		}

		accum_num += accum_inner_num;
		accum_den += accum_inner_den;
	}
	*num = accum_num;
	*den = accum_den;
}

void vif_filter1d_s(const float *f, const float *src, float *dst, float *tmpbuf, int w, int h, int src_stride, int dst_stride, int fwidth)
{

    int src_px_stride = src_stride / sizeof(float);
    int dst_px_stride = dst_stride / sizeof(float);

    /* if support avx */

#if ARCH_X86
    const unsigned flags = vmaf_get_cpu_flags();
    if ((flags & VMAF_X86_CPU_FLAG_AVX2) && (fwidth == 17 || fwidth == 9 || fwidth == 5 || fwidth == 3)) {
        convolution_f32_avx_s(f, fwidth, src, dst, tmpbuf, w, h,
                              src_px_stride, dst_px_stride);
        return;
    }
#endif

    /* fall back */

    float *tmp = aligned_malloc(ALIGN_CEIL(w * sizeof(float)), MAX_ALIGN);
    float fcoeff, imgcoeff;

    int i, j, fi, fj, ii, jj;

    for (i = 0; i < h; ++i) {
        /* Vertical pass. */
        for (j = 0; j < w; ++j) {
            float accum = 0;

            for (fi = 0; fi < fwidth; ++fi) {
                fcoeff = f[fi];

                ii = i - fwidth / 2 + fi;
                ii = ii < 0 ? -ii : (ii >= h ? 2 * h - ii - 1 : ii);

                imgcoeff = src[ii * src_px_stride + j];

                accum += fcoeff * imgcoeff;
            }

            tmp[j] = accum;
        }

        /* Horizontal pass. */
        for (j = 0; j < w; ++j) {
            float accum = 0;

            for (fj = 0; fj < fwidth; ++fj) {
                fcoeff = f[fj];

                jj = j - fwidth / 2 + fj;
                jj = jj < 0 ? -jj : (jj >= w ? 2 * w - jj - 1 : jj);

                imgcoeff = tmp[jj];

                accum += fcoeff * imgcoeff;
            }

            dst[i * dst_px_stride + j] = accum;
        }
    }

    aligned_free(tmp);
}

// Code optimized by adding intrinsic code for the functions,
// vif_filter1d_sq and vif_filter1d_sq

void vif_filter1d_sq_s(const float *f, const float *src, float *dst, float *tmpbuf, int w, int h, int src_stride, int dst_stride, int fwidth)
{

	int src_px_stride = src_stride / sizeof(float);
	int dst_px_stride = dst_stride / sizeof(float);

	/* if support avx */
	
#if ARCH_X86
    const unsigned flags = vmaf_get_cpu_flags();
    if ((flags & VMAF_X86_CPU_FLAG_AVX2) && (fwidth == 17 || fwidth == 9 || fwidth == 5 || fwidth == 3)) {
        convolution_f32_avx_sq_s(f, fwidth, src, dst, tmpbuf, w, h,
                                 src_px_stride, dst_px_stride);
        return;
    }
#endif

	/* fall back */

	float *tmp = aligned_malloc(ALIGN_CEIL(w * sizeof(float)), MAX_ALIGN);
	float fcoeff, imgcoeff;

	int i, j, fi, fj, ii, jj;

	for (i = 0; i < h; ++i) {
		/* Vertical pass. */
		for (j = 0; j < w; ++j) {
			float accum = 0;

			for (fi = 0; fi < fwidth; ++fi) {
				fcoeff = f[fi];

				ii = i - fwidth / 2 + fi;
				ii = ii < 0 ? -ii : (ii >= h ? 2 * h - ii - 1 : ii);

				imgcoeff = src[ii * src_px_stride + j];

				accum += fcoeff * (imgcoeff * imgcoeff);
			}

			tmp[j] = accum;
		}

		/* Horizontal pass. */
		for (j = 0; j < w; ++j) {
			float accum = 0;

			for (fj = 0; fj < fwidth; ++fj) {
				fcoeff = f[fj];

				jj = j - fwidth / 2 + fj;
				jj = jj < 0 ? -jj : (jj >= w ? 2 * w - jj - 1 : jj);

				imgcoeff = tmp[jj];

				accum += fcoeff * imgcoeff;
			}

			dst[i * dst_px_stride + j] = accum;
		}
	}

	aligned_free(tmp);
}

void vif_filter1d_xy_s(const float *f, const float *src1, const float *src2, float *dst, float *tmpbuf, int w, int h, int src1_stride, int src2_stride, int dst_stride, int fwidth)
{

	int src1_px_stride = src1_stride / sizeof(float);
	int src2_px_stride = src2_stride / sizeof(float);
	int dst_px_stride = dst_stride / sizeof(float);

	/* if support avx */

#if ARCH_X86
    const unsigned flags = vmaf_get_cpu_flags();
    if ((flags & VMAF_X86_CPU_FLAG_AVX2) && (fwidth == 17 || fwidth == 9 || fwidth == 5 || fwidth == 3)) {
        convolution_f32_avx_xy_s(f, fwidth, src1, src2, dst, tmpbuf, w, h,
                                 src1_px_stride, src2_px_stride, dst_px_stride);
        return;
    }
#endif

	/* fall back */

	float *tmp = aligned_malloc(ALIGN_CEIL(w * sizeof(float)), MAX_ALIGN);
	float fcoeff, imgcoeff, imgcoeff1, imgcoeff2;

	int i, j, fi, fj, ii, jj;

	for (i = 0; i < h; ++i) {
		/* Vertical pass. */
		for (j = 0; j < w; ++j) {
			float accum = 0;

			for (fi = 0; fi < fwidth; ++fi) {
				fcoeff = f[fi];

				ii = i - fwidth / 2 + fi;
				ii = ii < 0 ? -ii : (ii >= h ? 2 * h - ii - 1 : ii);

				imgcoeff1 = src1[ii * src1_px_stride + j];
				imgcoeff2 = src2[ii * src2_px_stride + j];

				accum += fcoeff * (imgcoeff1 * imgcoeff2);
			}

			tmp[j] = accum;
		}

		/* Horizontal pass. */
		for (j = 0; j < w; ++j) {
			float accum = 0;

			for (fj = 0; fj < fwidth; ++fj) {
				fcoeff = f[fj];

				jj = j - fwidth / 2 + fj;
				jj = jj < 0 ? -jj : (jj >= w ? 2 * w - jj - 1 : jj);

				imgcoeff = tmp[jj];

				accum += fcoeff * imgcoeff;
			}

			dst[i * dst_px_stride + j] = accum;
		}
	}

	aligned_free(tmp);
}
