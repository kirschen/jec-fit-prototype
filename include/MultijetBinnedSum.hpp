#pragma once

#include <FitBase.hpp>

#include <Morphing.hpp>
#include <Nuisances.hpp>

#include <TH1.h>
#include <TH1D.h>
#include <TH2.h>
#include <TProfile.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>


struct FracBin;


/**
 * \class MultijetBinnedSum
 * \brief Implements computation of the deviation of data from expectation in the multijet analysis
 * 
 * The deviation is computed as a chi^2 distance,
 *   chi^2 = sum_i (B^{Data}_i - B^{Sim}_i)^2 / (sigma^{Data}_i^2 + sigma^{Sim}_i^2),
 * where B_i is the mean balance observable in bin i in pt of the leading jet and sigma_i is its
 * statistical uncertainty. In data the mean balance observable is recomputed for the given jet
 * correction following the method described in [1-2].
 * [1] https://indico.cern.ch/event/646599/#50-on-the-way-to-an-updated-mu
 * [2] https://indico.cern.ch/event/656050/#65-comparison-of-different-app
 * 
 * Several systematic uncertainties are included. They are evaluated as multiplicative shifts in
 * B^{Sim}.
 */
class MultijetBinnedSum: public MeasurementBase
{
public:
    /// Supported methods of computation
    enum class Method
    {
        PtBal,
        MPF
    };

    /// Supported ReturnTypes for retrieving histograms over full range
    enum class HistReturnType
    {
        bal,
	recompBal,
        simBal,
	simBalShifted
    };
  
private:
    /// Auxiliary structure to aggregate data related to a single trigger bin
    struct TriggerBin
    {
        /**
         * \brief Binning in pt of the leading jet in data
         * 
         * The same binning is used for all data histograms and profiles.
         */
        std::vector<double> binning;
        
        /**
         * \brief Profiles of the balance observable in data and simulation
         * 
         * Binning of the profile in simulation defines bins to compute chi^2.
         */
        std::unique_ptr<TProfile> balProfile, simBalProfile;
        
        /// Distribution of pt of the leading jet in data
        std::unique_ptr<TH1> ptLead;
        
        /**
         * \brief Profile of pt of the leading jet in data
         * 
         * Used to obtain true mean pt in each bin.
         */
        std::unique_ptr<TProfile> ptLeadProfile;
        
        /// Sum of projections of pt of jets in bins of pt of the leading and other jets
        std::unique_ptr<TH2> ptJetSumProj;
        
        /**
         * \brief Squared uncertainty on the difference between mean balance observables in data
         * and simulation
         * 
         * Computed in the binning of simBalProfile.
         */
        std::vector<double> totalUnc2;
        
        /**
         * \brief Recomputed mean balance observable in data
         * 
         * Computed in the binning of simBalProfile.
         */
        mutable std::vector<double> recompBal;
        
        /**
         * \brief Systematic variations
         * 
         * The key of the map is the name of the nuisance parameter controlling the variation.
         * The variation is defined as a relative deviation from mean values of the balance
         * observable in simulation.
         */
        std::map<std::string, HistMorph> systVars;
    };
        
public:
    /// Constructor
    MultijetBinnedSum(std::string const &fileName, Method method, NuisanceDefinitions &nuisanceDefs);
    
public:
    /**
     * \brief Returns dimensionality of the deviation
     * 
     * Implemented from MeasurementBase.
     */
    virtual unsigned GetDim() const override;
    
    /**
     * \brief Builds a histogram of recomputed mean balance observable in data
     * 
     * The binning is as used for simulation.
     */
    TH1D GetRecompBalance(JetCorrBase const &corrector, Nuisances const &nuisances, HistReturnType histReturnType) const;
    
    /**
     * \brief Evaluates the deviation with the given jet corrector and set of nuisances
     * 
     * Implemented from MeasurementBase.
     */
    virtual double Eval(JetCorrBase const &corrector, Nuisances const &nuisances) const override;
    
    /**
     * \brief Selects a subrange of trigger bins to use
     * 
     * Only trigger bins whose (zero-based) indices are included in the range [begin, end) will be
     * considered for the computation of the deviation. The last given index is not included in the
     * range. It is optional, and if omitted, all trigger bins until the end will be used.
     * 
     * Normally all trigger bins should be considered. This method is intended for non-standard
     * experiments with the fit.
     */
    void SetTriggerBinRange(unsigned begin, unsigned end = -1);
    
private:
    /// Recomputes MPF in data for given trigger bin, 2D pt window, and jet correction
    static double ComputeMPF(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
    
    /// Recomputes pt balance in data for given trigger bin, 2D pt window, and jet correction
    static double ComputePtBal(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
    
    /// Recomputes mean balance observable in all trigger bins for the given jet correction
    void UpdateBalance(JetCorrBase const &corrector, Nuisances const &) const;
    
private:
    /// Method of computation
    Method method;
    
    /// Inputs for different trigger bins
    std::vector<TriggerBin> triggerBins;
    
    /**
     * \brief Selected subrange of trigger bins
     * 
     * The first bin is included in the range, the last one is not.
     */
    unsigned selectedTriggerBinsBegin, selectedTriggerBinsEnd;
    
    /// Jet pt threshold
    double minPt;
    
    /// Dimensionality of the deviation
    unsigned dimensionality;
};

