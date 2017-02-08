import sys, os
import argparse

#parser = argparse.ArgumentParser(description='Validation plots')
#parser.add_argument("dat", help="dat file (with unique basename)")
#parser.add_argument("names", nargs="+", help="comma separted pairs for labels and names (e.g. s1,\"Madgraph MC\"")
#parser.add_argument("--plotdir", help="outdir for plots", default="plots/")
#
#args = parser.parse_args()

sys.path.insert(0, os.getcwd() + '/python')
import plot
import ROOT

#plot.ndraw_entries = 100000
ROOT.gROOT.SetBatch(True)
ROOT.gStyle.SetOptStat(0)
ROOT.gStyle.SetOptTitle(0)

isMC = True
category = "Et_25-isEle-eleID_loose25nsRun22016Moriond"
#category = "Et_5-eleID_loose25nsRun22016Moriond"

lt_names = [ "5 - 75", "75 - 80", "80 - 85", "85 - 90", "90 - 95", "95 - 100",
				 "100 - 200", "200 - 400", "400 - 800", "800 - 2000", ]

inc_filename = "tmp/Moriond17_oldRegr/s1_chain.root"
inc_chain = ROOT.TFile.Open(inc_filename).Get("selected")

lt_binned_chains = []
for i in range(1,11):
	lt_binned_filename = "tmp/LTregrOld/s%d_chain.root" % i
	lt_binned_chain = ROOT.TFile.Open(lt_binned_filename).Get("selected")
	lt_binned_chains.append(lt_binned_chain)

lt_binned_filename = "tmp/LTregrOld/s_chain.root"
lt_binned_all = ROOT.TFile.Open(lt_binned_filename).Get("selected")

datafilename = "tmp/Moriond17_oldRegr/d_chain.root"
data_chain = ROOT.TFile.Open(datafilename).Get("selected")

#inclusive vs binned stack
#invmass

def comp(branchname, filename, binning, xlabel, ratio, nele, logx=False):
	inclusive = plot.GetTH1(inc_chain, branchname, isMC, binning, category, label="Inclusive Madgraph", usePU = False, useEleIDSF=nele)

	lt_hists = []
	lt_sum = 0
	for name, chain in zip(lt_names,lt_binned_chains):
		print "Getting", name
		h = plot.GetTH1(chain, branchname, isMC, binning, category, label= "L_{T} " + name + " GeV", usePU = False, useEleIDSF=nele)
		lt_hists.append(h)
		lt_sum += h.Integral()


	plot.ColorData(inclusive)
	plot.ColorMCs(lt_hists)
	inclusive.Scale(1./inclusive.Integral())
	for h in lt_hists:
		h.Scale(1./lt_sum)

	lt_hists.reverse()
	plot.PlotDataMC(inclusive, lt_hists, "plots/validation/LTBinned/", "LT_comparison_" + filename, xlabel=xlabel + " [GeV]", ylabel="Fraction", ylabel_unit="GeV", stack_mc=True, ratio=ratio, logx=logx)

	data_h = plot.GetTH1(data_chain, branchname, False, binning, category, label="Data OldRegr", usePU = False)
	lt_binned_all_h = plot.GetTH1(lt_binned_all, branchname, isMC, binning, category, label="LTBinned Madgraph", usePU = False, useEleIDSF=nele)

	mc_hists = [lt_binned_all_h, inclusive]
	plot.ColorData(data_h)
	plot.ColorMCs(mc_hists)
	plot.Normalize(data_h, mc_hists)
	plot.PlotDataMC(data_h, mc_hists, "plots/validation/LTBinned/", "LT_dataMC__" + filename, xlabel=xlabel + " [GeV]", ylabel="Events", ylabel_unit="GeV", ratio=ratio, logx=logx)

branchname = "invMass_ECAL_ele"
filename = "invMass_ECAL_ele"
binning = "(100,80,100)"
xlabel  = "M_{ee}"
comp(branchname, filename, binning, xlabel, (.9,1.1), 2)

branchname = "energy_ECAL_ele[0]/cosh(etaSCEle[0]) + energy_ECAL_ele[1]/cosh(etaSCEle[1])"
filename = "LT"
binning = "(100,40,200)"
xlabel  = "L_{T}"
comp(branchname, filename, binning, xlabel, (.8,1.2), 2)

binning = "(100,40,300)"
comp(branchname, filename, binning, xlabel, (.8,1.2), 2, logx=True)

branchname = "energy_ECAL_ele/cosh(etaSCEle)"
filename = "ET"
binning = "(100,1,81)"
xlabel  = "E_{T}"
comp(branchname, filename, binning, xlabel, (.8, 1.2), 1)

branchname = "etaSCEle"
filename = "eta"
binning = "(100,-2.5,2.5)"
xlabel  = "#eta_{SC}"
comp(branchname, filename, binning, xlabel, True, 1)

branchname = "R9Ele"
filename = "R9"
binning = "(100,.3,1)"
xlabel  = "R_{9}"
comp(branchname, filename, binning, xlabel, True, 1)


sys.exit()
# 2d ET data
import numpy as np

bins = [
	25.0,    26.0,     27.0,     27.875,  28.75,
	29.625,  30.375,   31.125,   31.875,  32.625,
	33.375,  34.125,   34.75,    35.375,  36.0,
	36.625,  37.25,    37.875,   38.5,    39.125,
	39.625,  40.125,   40.625,   41.125,  41.625,
	42.125,  42.625,   43.125,   43.625,  44.125,
	44.625,  45.125,   45.625,   46.125,  46.75,
	47.5,    48.5,     49.875,   51.625,  53.875,
	56.75,   60.5,     65.25,    71.5,    79.875,
	91.875,  110.625,  147.625,  2000.0
	]

bins = np.array(bins)
ROOT.gStyle.SetOptTitle(1)

branchname = "energy_ECAL_ele[0]/cosh(etaSCEle[0]):energy_ECAL_ele[1]/cosh(etaSCEle)[1]"
inc_et= ROOT.TH2F("inc_et", "", len(bins) - 1, bins, len(bins) - 1, bins)
plot.GetTH1(inc_chain, branchname, isMC, histname="inc_et", category=category, label="Inclusive Madgraph", usePU = False, useEleIDSF = False)
plot.FoldTH2(inc_et)

data_et= ROOT.TH2F("data_et", "", len(bins) - 1, bins, len(bins) - 1, bins)
plot.GetTH1(data_chain, branchname, False, histname="data_et", category=category, label="Data", usePU = False, useEleIDSF = False)
plot.FoldTH2(data_et)

ltbinned_et= ROOT.TH2F("ltbinned_et", "", len(bins) - 1, bins, len(bins) - 1, bins)
plot.GetTH1(lt_binned_all, branchname, isMC, histname="ltbinned_et", category=category, label="LT Binned Madgraph", usePU = False, useLT=True, useEleIDSF = False)
plot.FoldTH2(ltbinned_et)

ltbinned_et_entries = ROOT.TH2F("ltbinned_et_entries", "", len(bins) - 1, bins, len(bins) - 1, bins)
plot.GetTH1(lt_binned_all, branchname, isMC, histname="ltbinned_et_entries", category=category, label="LT Binned Madgraph Unweighted", usePU = False, useLT=False, useEleIDSF = False)
plot.FoldTH2(ltbinned_et_entries)

for h in [inc_et, data_et, ltbinned_et, ltbinned_et_entries]:
	print h.GetName()
	for i in xrange(1, h.GetXaxis().GetNbins()+1):
		print "low={}, hi={}, value={}".format(h.GetXaxis().GetBinLowEdge(i), h.GetXaxis().GetBinLowEdge(i+1),  h.GetBinContent(i,i))

data_et.GetXaxis().SetRangeUser(data_et.GetXaxis().GetXmin(), 200)
data_et.GetYaxis().SetRangeUser(data_et.GetYaxis().GetXmin(), 200)
plot.Draw2D(inc_et,               "E_{T}",  "ET_inc_2d",               plotdir="plots/validation/LTBinned/",  logx=True,  logy=True)
plot.Draw2D(data_et,              "E_{T}",  "ET_data_2d",              plotdir="plots/validation/LTBinned/",  logx=True,  logy=True)
plot.Draw2D(ltbinned_et,          "E_{T}",  "ET_ltbinned_2d",          plotdir="plots/validation/LTBinned/",  logx=True,  logy=True)
plot.Draw2D(ltbinned_et_entries,  "E_{T}",  "ET_ltbinned_entries_2d",  plotdir="plots/validation/LTBinned/",  logx=True,  logy=True)
