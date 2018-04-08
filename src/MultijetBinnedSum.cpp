#include <MultijetBinnedSum.hpp>

#include <Rebin.hpp>

#include <TFile.h>
#include <TKey.h>
#include <TVectorD.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <utility>


MultijetBinnedSum::MultijetBinnedSum(std::string const &fileName,
  MultijetBinnedSum::Method method_):
    method(method_)
{
    std::string methodLabel;
    
    if (method == Method::PtBal)
        methodLabel = "PtBal";
    else if (method == Method::MPF)
        methodLabel = "MPF";
    
    
    std::unique_ptr<TFile> inputFile(TFile::Open(fileName.c_str()));
    
    if (not inputFile or inputFile->IsZombie())
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::MultijetBinnedSum: Failed to open file \"" <<
          fileName << "\".";
        throw std::runtime_error(message.str());
    }
    
    
    // Read the jet pt threshold. It is not a free parameter and must be set to the same value as
    //used to construct the inputs. For the pt balance method it affects the definition of the
    //balance observable in simulation (while in data it can be recomputed for any not too low
    //threshold). In the case of the MPF method the definition of the balance observable in both
    //data and simulation is affected.
    auto ptThreshold = dynamic_cast<TVectorD *>(inputFile->Get(("MinPt" + methodLabel).c_str()));
    
    if (not ptThreshold or ptThreshold->GetNoElements() != 1)
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::MultijetBinnedSum: Failed to read jet pt threshold " <<
          "from file \"" << fileName << "\".";
        throw std::runtime_error(message.str());
    }
    
    minPt = (*ptThreshold)[0];
    
    
    // Loop over directories in the input file
    TIter fileIter(inputFile->GetListOfKeys());
    TKey *key;
    
    while ((key = dynamic_cast<TKey *>(fileIter())))
    {
        if (strcmp(key->GetClassName(), "TDirectoryFile") != 0)
            continue;
        
        TDirectoryFile *directory = dynamic_cast<TDirectoryFile *>(key->ReadObj());
        
        for (auto const &name: std::initializer_list<std::string>{"Sim" + methodLabel + "Profile",
          "PtLead", "PtLeadProfile", methodLabel + "Profile", "PtJetSumProj"})
        {
            if (not directory->Get(name.c_str()))
            {
                std::ostringstream message;
                message << "MultijetBinnedSum::MultijetBinnedSum: Directory \"" <<
                  key->GetName() << "\" in file \"" << fileName <<
                  "\" does not contain required key \"" << name << "\".";
                throw std::runtime_error(message.str());
            }
        }
        
        
        TriggerBin bin;
        
        bin.simBalProfile.reset(dynamic_cast<TProfile *>(
          directory->Get(("Sim" + methodLabel + "Profile").c_str())));
        bin.balProfile.reset(dynamic_cast<TProfile *>(
          directory->Get((methodLabel + "Profile").c_str())));
        bin.ptLead.reset(dynamic_cast<TH1 *>(directory->Get("PtLead")));
        bin.ptLeadProfile.reset(dynamic_cast<TProfile *>(directory->Get("PtLeadProfile")));
        bin.ptJetSumProj.reset(dynamic_cast<TH2 *>(directory->Get("PtJetSumProj")));
        
        bin.simBalProfile->SetDirectory(nullptr);
        bin.balProfile->SetDirectory(nullptr);
        bin.ptLead->SetDirectory(nullptr);
        bin.ptLeadProfile->SetDirectory(nullptr);
        bin.ptJetSumProj->SetDirectory(nullptr);
        
        triggerBins.emplace_back(std::move(bin));
    }
    
    inputFile->Close();
    
    if (triggerBins.empty())
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::MultijetBinnedSum: No data read from file \"" <<
          fileName << "\".";
        throw std::runtime_error(message.str());
    }
    
    
    // Construct remaining fields in trigger bins
    for (auto &bin: triggerBins)
    {
        // Save binning in data in a handy format
        bin.binning.reserve(bin.ptLead->GetNbinsX() + 1);
        
        for (int i = 1; i <= bin.ptLead->GetNbinsX() + 1; ++i)
            bin.binning.emplace_back(bin.ptLead->GetBinLowEdge(i));
        
        
        // Compute combined (squared) uncertainty on the balance observable in data and simulation.
        //The data profile is rebinned with the binning used for simulation. This is done assuming
        //that bin edges of the two binnings are aligned, which should normally be the case.
        std::unique_ptr<TH1> balRebinned(bin.balProfile->Rebin(
          bin.simBalProfile->GetNbinsX(), "",
          bin.simBalProfile->GetXaxis()->GetXbins()->GetArray()));
        
        for (int i = 1; i <= bin.simBalProfile->GetNbinsX() + 1; ++i)
        {
            double const unc2 = std::pow(bin.simBalProfile->GetBinError(i), 2) +
              std::pow(balRebinned->GetBinError(i), 2);
            bin.totalUnc2.emplace_back(unc2);
        }
        
        
        // Initialize recomputed mean balance observable with dummy values
        bin.recompBal.resize(bin.simBalProfile->GetNbinsX());
    }
    
    
    // Set the range of trigger bins to include all of them
    selectedTriggerBinsBegin = 0;
    selectedTriggerBinsEnd = triggerBins.size();
    
    
    // Precompute dimensionality. It is given by binning of simulation.
    dimensionality = 0;
    
    for (auto const &bin: triggerBins)
        dimensionality += bin.simBalProfile->GetNbinsX();
}


unsigned MultijetBinnedSum::GetDim() const
{
    return dimensionality;
}


TH1D MultijetBinnedSum::GetRecompBalance(JetCorrBase const &corrector, Nuisances const &nuisances, HistReturnType histReturnType)
  const
{
    // An auxiliary structure to aggregate information about a single bin. Consists of the lower
    //bin edge, bin content, and its uncertainty.
    using Bin = std::tuple<double, double, double>;
    
    
    // Recompute mean balance observables
    UpdateBalance(corrector, nuisances);
    
    
    // Read recomputed mean balance observables for all bins
    std::vector<Bin> bins;
    bins.reserve(dimensionality);
    
    double upperBoundary = -std::numeric_limits<double>::infinity();
    
    for (unsigned iTriggerBin = selectedTriggerBinsBegin; iTriggerBin < selectedTriggerBinsEnd;
      ++iTriggerBin)
    {
        auto const &triggerBin = triggerBins[iTriggerBin];
        
        auto const &simBalProfile = triggerBin.simBalProfile;

	std::unique_ptr<TH1> balRebinned(triggerBin.balProfile->Rebin(
	  triggerBin.simBalProfile->GetNbinsX(), "",
	  triggerBin.simBalProfile->GetXaxis()->GetXbins()->GetArray()));

        for (unsigned i = 0; i < triggerBin.recompBal.size(); ++i){
	  double ptLead = triggerBin.simBalProfile->GetBinCenter(i+1);
	  double shifts=0;
	  switch(histReturnType){
	  case HistReturnType::bal: 
            bins.emplace_back(std::make_tuple(simBalProfile->GetBinLowEdge(i + 1),
					      balRebinned->GetBinContent(i+1), balRebinned->GetBinError(i+1)));
	    break;
	  case HistReturnType::recompBal: //triggerBin.recompBal is a plain vector, thus the offset of 1 w.r.t. bin contents
            if (method == Method::PtBal){
              for(unsigned MJBn_i = 0;  MJBn_i<nuisances.MJB_NuisanceCollection.size(); ++MJBn_i){
                shifts+= * (std::get<double*>(nuisances.MJB_NuisanceCollection.at(MJBn_i))) * (std::get<TF1*>(nuisances.MJB_NuisanceCollection.at(MJBn_i)))->Eval(ptLead);
              }
            }
            else if (method == Method::MPF){
              for(unsigned MPFn_i = 0;  MPFn_i<nuisances.MPF_NuisanceCollection.size(); ++MPFn_i){
                shifts+= * (std::get<double*>(nuisances.MPF_NuisanceCollection.at(MPFn_i))) * (std::get<TF1*>(nuisances.MPF_NuisanceCollection.at(MPFn_i)))->Eval(ptLead);
              }
            }
	    
            bins.emplace_back(std::make_tuple(simBalProfile->GetBinLowEdge(i + 1),
					      triggerBin.recompBal[i]+shifts, std::sqrt(triggerBin.totalUnc2[i])));
	    break;
	  case HistReturnType::simBal:
            bins.emplace_back(std::make_tuple(simBalProfile->GetBinLowEdge(i + 1),
					      simBalProfile->GetBinContent(i+1), simBalProfile->GetBinError(i+1)));
	    break;
	  }

	}
        
        
        double const lastEdge = simBalProfile->GetBinLowEdge(simBalProfile->GetNbinsX() + 1);
        
        if (lastEdge > upperBoundary)
            upperBoundary = lastEdge;
    }
    
    
    // Different trigger bins might not have been ordered in pt. Sort the constructed list of bins.
    std::sort(bins.begin(), bins.end(),
      [](auto const &lhs, auto const &rhs){return (std::get<0>(lhs) < std::get<0>(rhs));});
    
    
    // Construct a histogram from the collection of bins
    std::vector<double> edges;
    edges.reserve(bins.size() + 1);
    
    for (unsigned i = 0; i < bins.size(); ++i)
        edges.emplace_back(std::get<0>(bins[i]));
    
    edges.emplace_back(upperBoundary);
    
    TH1D hist("RecompBalance", "", edges.size() - 1, edges.data());
    hist.SetDirectory(nullptr);
    
    for (unsigned i = 0; i < bins.size(); ++i)
    {
        hist.SetBinContent(i + 1, std::get<1>(bins[i]));
        hist.SetBinError(i + 1, std::get<2>(bins[i]));
    }
    
    
    return hist;
}


double MultijetBinnedSum::Eval(JetCorrBase const &corrector, Nuisances const &nuisances) const
{
    UpdateBalance(corrector, nuisances);
    double chi2 = 0.;
    
    for (unsigned iTriggerBin = selectedTriggerBinsBegin; iTriggerBin < selectedTriggerBinsEnd;
      ++iTriggerBin)
    {
        auto const &triggerBin = triggerBins[iTriggerBin];
        
        for (unsigned binIndex = 1; binIndex <= triggerBin.recompBal.size(); ++binIndex)
        {
            double const meanBal = triggerBin.recompBal[binIndex - 1];
            double const simMeanBal = triggerBin.simBalProfile->GetBinContent(binIndex);
            double ptLead = triggerBin.simBalProfile->GetBinCenter(binIndex);
            double shifts=0;
            if(std::isnan(meanBal) || std::isnan(simMeanBal)){
              std::cout << "\n \033[1;31m ERROR: \033[0m\n NaN in binIndex" << binIndex << " in triggerBin " << iTriggerBin<< std::endl;
              std::cout << "will skip this bin and try to continue" << std::endl;
              continue;
            }

            if (method == Method::PtBal){
              for(unsigned MJBn_i = 0;  MJBn_i<nuisances.MJB_NuisanceCollection.size(); ++MJBn_i){
                shifts+= * (std::get<double*>(nuisances.MJB_NuisanceCollection.at(MJBn_i))) * (std::get<TF1*>(nuisances.MJB_NuisanceCollection.at(MJBn_i)))->Eval(ptLead);
              }
            }
            else if (method == Method::MPF){
              for(unsigned MPFn_i = 0;  MPFn_i<nuisances.MPF_NuisanceCollection.size(); ++MPFn_i){
                shifts+= * (std::get<double*>(nuisances.MPF_NuisanceCollection.at(MPFn_i))) * (std::get<TF1*>(nuisances.MPF_NuisanceCollection.at(MPFn_i)))->Eval(ptLead);
              }
            }
            //          std::cout << "ptLead " << ptLead << " meanBal " << meanBal << " shifts " << shifts  << " simMeanBal " << simMeanBal  << " totalunc2 " << triggerBin.totalUnc2[binIndex - 1] << " chi2 " << chi2 <<  std::endl;
            
            chi2 += std::pow(meanBal +shifts - simMeanBal, 2) / triggerBin.totalUnc2[binIndex - 1];

            
        }
    }
    
    return chi2;
}


void MultijetBinnedSum::SetTriggerBinRange(unsigned begin, unsigned end)
{
    unsigned const numTriggerBins = triggerBins.size();
    
    if (end == unsigned(-1))
        end = numTriggerBins;
    
    
    // Sanity checks
    if (begin > numTriggerBins)
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::SetTriggerBinRange: Requested starting index " << begin <<
          "is bigger than the number of available trigger bins " << numTriggerBins << ".";
        throw std::runtime_error(message.str());
    }
    
    if (end > numTriggerBins)
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::SetTriggerBinRange: Requested ending index " << end <<
          "is bigger than the number of available trigger bins " << numTriggerBins << ".";
        throw std::runtime_error(message.str());
    }
    
    if (begin >= end)
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::SetTriggerBinRange: Range [" << begin << ", " << end <<
          ") selects nothing.";
        throw std::runtime_error(message.str());
    }
    
    
    selectedTriggerBinsBegin = begin;
    selectedTriggerBinsEnd = end;
    
    
    // Have to recompute cached dimensionality
    dimensionality = 0;
    
    for (unsigned i = selectedTriggerBinsBegin; i < selectedTriggerBinsEnd; ++i)
        dimensionality += triggerBins[i].simBalProfile->GetNbinsX();
}


double MultijetBinnedSum::ComputeMPF(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
  FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector)
{
    double sumBal = 0., sumWeight = 0.;
    
    // Loop over bins in ptlead
    for (unsigned iPtLead = ptLeadStart.index; iPtLead <= ptLeadEnd.index; ++iPtLead)
    {
        unsigned numEvents = triggerBin.ptLead->GetBinContent(iPtLead);
        
        if (numEvents == 0)
            continue;
        
        double const ptLead = triggerBin.ptLeadProfile->GetBinContent(iPtLead);
        
        
        // Sum over other jets. Consider separately the starting bin, which is only partly
        //included, and the remaining ones
        double sumJets = 0.;
        
        double pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(ptJetStart.index);
        double s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, ptJetStart.index);
        sumJets += (1 - corrector.Eval(pt)) * s * ptJetStart.frac;
        
        for (int iPtJ = ptJetStart.index + 1; iPtJ < triggerBin.ptJetSumProj->GetNbinsY() + 1;
          ++iPtJ)
        {
            pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(iPtJ);
            s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, iPtJ);
            sumJets += (1 - corrector.Eval(pt)) * s;
        }
        
        
        // The first and the last bins are only partially included. Find the inclusion fraction for
        //the current bin. The computation holds true also when the loop runs over only a single
        //bin in ptlead only.
        double fraction = 1.;
        
        if (iPtLead == ptLeadStart.index)
            fraction = ptLeadStart.frac;
        else if (iPtLead == ptLeadEnd.index)
            fraction = ptLeadEnd.frac;
        
        
        sumBal += triggerBin.balProfile->GetBinContent(iPtLead) * numEvents /
          corrector.Eval(ptLead) * fraction;
        sumBal += sumJets / (ptLead * corrector.Eval(ptLead)) * fraction;
        sumWeight += numEvents * fraction;
    }
    
    return sumBal / sumWeight;
}


double MultijetBinnedSum::ComputePtBal(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
  FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector)
{
    double sumBal = 0., sumWeight = 0.;
    
    // Loop over bins in ptlead
    for (unsigned iPtLead = ptLeadStart.index; iPtLead <= ptLeadEnd.index; ++iPtLead)
    {
        unsigned numEvents = triggerBin.ptLead->GetBinContent(iPtLead);
        
        if (numEvents == 0)
            continue;
        
        double const ptLead = triggerBin.ptLeadProfile->GetBinContent(iPtLead);
        
        
        // Sum over other jets. Consider separately the starting bin, which is only partly
        //included, and the remaining ones
        double sumJets = 0.;
        
        double pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(ptJetStart.index);
        double s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, ptJetStart.index);
        sumJets += s * corrector.Eval(pt) * ptJetStart.frac;
        
        for (int iPtJ = ptJetStart.index + 1; iPtJ < triggerBin.ptJetSumProj->GetNbinsY() + 1;
          ++iPtJ)
        {
            pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(iPtJ);
            s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, iPtJ);
            sumJets += s * corrector.Eval(pt);
        }
        
        
        // The first and the last bins are only partially included. Find the inclusion fraction for
        //the current bin. The computation holds true also when the loop runs over only a single
        //bin in ptlead only.
        double fraction = 1.;
        
        if (iPtLead == ptLeadStart.index)
            fraction = ptLeadStart.frac;
        else if (iPtLead == ptLeadEnd.index)
            fraction = ptLeadEnd.frac;
        
        
        sumBal += sumJets / (ptLead * corrector.Eval(ptLead)) * fraction;
        sumWeight += numEvents * fraction;
    }
    
    return -sumBal / sumWeight;
}


void MultijetBinnedSum::UpdateBalance(JetCorrBase const &corrector, Nuisances const &) const
{
    double minPtUncorr = corrector.UndoCorr(minPt);
    
    if (triggerBins.front().ptJetSumProj->GetYaxis()->FindFixBin(minPtUncorr) == 0)
    {
        std::ostringstream message;
        message << "MultijetBinnedSum::UpdateBalance: With the current correction " <<
          "jet threshold (" << minPt << " -> " << minPtUncorr <<
          " GeV) falls in the underflow bin.";
        throw std::runtime_error(message.str());
    }
    
    
    for (unsigned iTriggerBin = selectedTriggerBinsBegin; iTriggerBin < selectedTriggerBinsEnd;
      ++iTriggerBin)
    {
        auto const &triggerBin = triggerBins[iTriggerBin];
        
        // The binning in pt of the leading jet in the profile for simulation corresponds to
        //corrected jets. Translate it into a binning in uncorrected pt.
        std::vector<double> uncorrPtBinning;
        
        for (int i = 1; i <= triggerBin.simBalProfile->GetNbinsX() + 1; ++i)
        {
            double const pt = triggerBin.simBalProfile->GetBinLowEdge(i);
            uncorrPtBinning.emplace_back(corrector.UndoCorr(pt));
        }
        
        
        // Build a map from this translated binning to the fine binning in data histograms. It
        //accounts both for the migration in pt of the leading jet due to the jet correction and
        //the typically larger size of bins used for computation of chi2.
        auto binMap = mapBinning(triggerBin.binning, uncorrPtBinning);
        
        // Under- and overflow bins in pt are included in other trigger bins and must be dropped
        binMap.erase(0);
        binMap.erase(triggerBin.simBalProfile->GetNbinsX() + 1);
        
        
        // Find bin in pt of other jets that contains minPtUncorr, and the corresponding inclusion
        //fraction
        auto const *axis = triggerBin.ptJetSumProj->GetYaxis();
        unsigned const minPtBin = axis->FindFixBin(minPtUncorr);
        double const minPtFrac = (minPtUncorr - axis->GetBinLowEdge(minPtBin)) /
          axis->GetBinWidth(minPtBin);
        FracBin const ptJetStart{minPtBin, 1. - minPtFrac};
        
        
        // Compute mean balance with the translated binning
        for (auto const &binMapPair: binMap)
        {
            auto const &binIndex = binMapPair.first;
            auto const &binRange = binMapPair.second;
            
            double meanBal;
            
            if (method == Method::PtBal)
                meanBal = ComputePtBal(triggerBin, binRange[0], binRange[1], ptJetStart, corrector);
            else
                meanBal = ComputeMPF(triggerBin, binRange[0], binRange[1], ptJetStart, corrector);
            if(std::isnan(meanBal))std::cout << "NaN in binIndex" << binIndex << std::endl;
            triggerBin.recompBal[binIndex - 1] = meanBal;
        }
    }
}
