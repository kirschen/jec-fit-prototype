#include <ZJetRun1.hpp>

#include <cmath>
#include <memory>
#include <sstream>

#include <TFile.h>
#include <TH1.h>

using namespace std::string_literals;


ZJetRun1::ZJetRun1(std::string const &fileName, Method method)
{
    std::string methodLabel;
    
    if (method == Method::PtBal)
        methodLabel = "ptbal";
    else if (method == Method::MPF)
        methodLabel = "mpf";
    
    std::unique_ptr<TFile> inputFile(TFile::Open(fileName.c_str()));
    
    if (not inputFile or inputFile->IsZombie())
    {
        std::ostringstream message;
        message << "ZJetRun1::ZJetRun1: Failed to open file \"" << fileName << "\".";
        throw std::runtime_error(message.str());
    }
    
    std::unique_ptr<TH1D> extrapRatioZ(dynamic_cast<TH1D *>(
      inputFile->Get((methodLabel + "_Ratio_eta_0-13_zpt_30-Inf_alpha_0_L1L2L3").c_str())));

    extrapRatioZ->SetDirectory(nullptr);
    inputFile->Close();
  
  
    bins.reserve(extrapRatioZ->GetNbinsX());
    
    for (int i = 1; i < extrapRatioZ->GetNbinsX(); ++i)
    //^ The last bin of the histogram is excluded temporarily because of a problem with inputs
    {
        PtBin bin;
        bin.ptZ = extrapRatioZ->GetBinCenter(i);
        bin.balanceRatio = extrapRatioZ->GetBinContent(i);
        bin.unc2 = pow(extrapRatioZ->GetBinError(i), 2);
        
        bins.push_back(bin);
    }
}


unsigned ZJetRun1::GetDim() const
{
    return bins.size();
}


double ZJetRun1::Eval(JetCorrBase const &corrector, Nuisances const &) const
{
    double chi2 = 0.;
    
    for (auto const &bin: bins)
    {
        // Assume that pt of the jet is the same as pt of the Z
        chi2 += std::pow(bin.balanceRatio - 1 / corrector.Eval(bin.ptZ), 2) / bin.unc2;
    }
    
    return chi2;
}
