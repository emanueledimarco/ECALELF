import sys, os
import argparse

parser = argparse.ArgumentParser(description='Standard Data MC plots')
parser.add_argument("category", help="category string")
parser.add_argument("binning", help="(nbins,low,hi)")
parser.add_argument("branch",nargs="*", help="Branch names to plot")
parser.add_argument("-d", "--data", action="append", help="specify data files, labels [and branches]. FILE,LABEL[,BRANCH[,ENERGYBRANCH]]")
parser.add_argument("-s", "--mc",   action="append", help="specify mc files, labels [and branches]. FILE,LABEL[,BRANCH[,ENERGYBRANCH]]")
parser.add_argument("-n", "--name", help="outfile base name (default=branchname)")
parser.add_argument("-x", "--xlabel", help="x axis label (default=branchname)")
parser.add_argument("--plotdir", help="outdir for plots", default="plots/")
parser.add_argument("--noPU", help="no pileup weights", default=False, action="store_true")
parser.add_argument("--noEleIDSF", help="no EleIDSF weights", default=False, action="store_true")

args = parser.parse_args()

sys.path.insert(0, os.getcwd() + '/python')
import plot
import ROOT

#plot.ndraw_entries = 100000
ROOT.gROOT.SetBatch(True)
ROOT.gStyle.SetOptStat(0)
ROOT.gStyle.SetOptTitle(0)

def MakeTH1s(arg, branchname, isMC, binning, category):
	hs = []
	for a in arg:
		parse = a.split(',')
		filename, label = parse[:2]
		if len(parse) >2: branchname = parse[2]
		elif not branchname:
			raise Exception("branch not defined for " + filename + ' ' + label)
		if len(parse) >3:
			energybranchname = parse[3]
		else:
			energybranchname = None


		chain = ROOT.TFile.Open(filename).Get("selected")
		hs.append( plot.GetTH1(chain, branchname, isMC, binning, category, label=label , energyBranchName = energybranchname, usePU= not args.noPU, useEleIDSF = not args.noEleIDSF))
	return hs

#eleID = "eleID_loose25nsRun22016Moriond"
#category = "Et_25-" + eleID

category = "absEta_0_1-gold-EtLeading_32-EtSubLeading_20-noPF-isEle-eleID_cutBasedElectronID|Spring15|25ns|V1|standalone|loose"
category = args.category
binning = args.binning

if not args.branch:
	args.branch = [""]

for branchname in args.branch:
	if not args.name:
		args.name = branchname
	if not args.xlabel:
		args.xlabel = branchname

	if args.data:
		print "[STATUS] Data: " + branchname
		isMC = False
		data_hs = MakeTH1s(args.data, branchname, isMC, binning, category)
		plot.ColorData(data_hs)
	else:
		data_hs = []

	if args.mc:
		print "[STATUS] MC: " +  branchname
		isMC = True
		mc_hs = MakeTH1s(args.mc, branchname, isMC, binning, category)
		plot.ColorMCs(mc_hs)
	else:
		mc_hs = []

	plot.Normalize(data_hs, mc_hs)
	plot.PlotDataMC(data_hs, mc_hs, args.plotdir, args.name, xlabel=args.xlabel, ylabel="Events", ylabel_unit="GeV", logy = False)
	print "[STATUS] Done: " + branchname
