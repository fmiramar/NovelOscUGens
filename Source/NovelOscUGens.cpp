// SPDX-License-Identifier: GPL-3.0-or-later
#include "NovelOscUGens.hpp"

InterfaceTable* ft = nullptr;

PluginLoad(NovelOscUGens)
{
    ft = inTable;
    registerHarmonicStrideOsc();
    registerTableMorph2D();
    registerTZSplitOsc();
    registerScaleSpreadOsc();
    registerSpectralBasisOsc();
    registerPartialClusterOsc();
    registerSyncRingOsc();
    registerPMNetworkOsc();
    registerClusterPMOsc();
    registerScannedNetworkOsc();
    registerVectorPhaseOsc();
    registerSpectralFrameOsc();
    registerRippleFormantOsc();
    registerShiftLogicOsc();
    registerQuadratureFeedbackOsc();
    registerSyncModeOsc();
    registerTableMorph3D();
    registerBiroPitchRegister();
}
