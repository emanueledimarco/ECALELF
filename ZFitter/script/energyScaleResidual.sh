#!/bin/bash
source script/functions.sh
source script/bash_functions_calibration.sh
#shopt -s expand_aliases
#source ~/.bashrc

# energy scale derived in different steps:
# - time dependence (step1)
# - material dependence  (step2)
# - smearings (step4)
# - quick likelihood scan in Et bins (profile only) (step5)
# - first Et minimization (step6)
# - second Et minimization (step7)
# - Et minization (closure test) (step8)
# - gain study (step9)

index= #useless?
eos_path=data/smearerCat
tmp_path=`pwd`
baseDir=${tmp_path}/test
eosDir=${eos_path}/test
printDir=www
updateOnly="--updateOnly --fit_type_value=1" # --profileOnly --initFile=init.txt"
commonCut="EtSingleEle_10"
#commonCut=Et_20-noPF #Standard common Cuts for Z calibration
#commonCut=Et_30-noPF #Et_30 for 0 T calibration
#default selection is loose25nsRun22016Moriond #you can change this via steps_maker.sh
selection="LooseEleID" #cutBasedElectronID-Spring15-25ns-V1-standalone-loose
invMass_var="mZ1"
#invMass_var=invMass_SC_pho_regrCorr #you can change this via script (steps_maker.sh)
Et_smear=
###########################################################
regionFileStep1=data/regions/scaleStep1.dat

regionFileStep2EB=data/regions/scaleStep2smearing_1.dat
regionFileStep2EE=data/regions/scaleStep2smearing_2.dat

regionFileStep4EBEE=data/regions/scaleStep4smearing_0.dat
regionFileStep4EBEE_Hgg=data/regions/scaleStep0_Hgg.dat
regionFileStep4EBEE_r9=data/regions/scaleStep0_r9.dat
regionFileStep4Inclusive=data/regions/scaleStep4_Inclusive.dat
regionFileStep4EB=data/regions/scaleStep4smearing_1.dat
regionFileStep4EE=data/regions/scaleStep4smearing_2.dat
##with R9 reweight
#regionFileStep4EB=data/regions/scaleStep4smearing_1_R9prime.dat
#regionFileStep4EE=data/regions/scaleStep4smearing_2_R9prime.dat
#regionFileStep4EB=data/regions/scaleStep0.dat
#regionFileStep4EE=data/regions/scaleStep0_bis.dat
#regionFileStep4EB_93=data/regions/scaleStep4smearing_1_R9prime_93.dat
#regionFileStep4EE_93=data/regions/scaleStep4smearing_2_R9prime_93.dat
#regionFileStep4EB_95=data/regions/scaleStep4smearing_1_R9prime_95.dat
#regionFileStep4EE_95=data/regions/scaleStep4smearing_2_R9prime_95.dat
regionFileStep4EB_93=data/regions/scaleStep4smearing_1_R9_93.dat
regionFileStep4EE_93=data/regions/scaleStep4smearing_2_R9_93.dat
regionFileStep4EB_95=data/regions/scaleStep4smearing_1_R9_95.dat
regionFileStep4EE_95=data/regions/scaleStep4smearing_2_R9_95.dat

regionFileStep5EB=data/regions/scaleStep2smearing_8.dat
regionFileStep5EE=data/regions/scaleStep2smearing_9.dat

regionFileStep6EB=data/regions/scaleStep6smearing_1.dat
regionFileStep6EE=data/regions/scaleStep6smearing_2.dat

#############################################################
usage(){
    echo "`basename $0` [options]" 
    echo " -f arg            config file"
#    echo " --regionFile arg             "
    echo " --runRangesFile arg           "
    echo " --selection arg (=$selection)"
    echo " --invMass_var arg"
    echo " --scenario arg"
    echo " --commonCut arg (=$commonCut)"
    echo " --step arg: 1, 2, 2fit, 3, 3weight, slide, 3stability, syst, 1-2,1-3,1-3stability and all ordered combination"
    echo " --index arg"
}
#------------------------------ parsing


# options may be followed by one colon to indicate they have a required argument
if ! options=$(getopt -u -o hf:s: -l help,regionFile:,runRangesFile:,selection:,invMass_var:,scenario:,step:,baseDir:,commonCut:,index:,force -- "$@")
then
    # something went wrong, getopt will put out an error message for us
    exit 1
fi

set -- $options

while [ $# -gt 0 ]
do
    case $1 in
	-h|--help) usage; exit 0;;
	-f) configFile=$2; shift;;
	-s|--step) STEP=$2; shift;;
	--force) FORCE=y;;
	--invMass_var) invMass_var=$2; echo "[OPTION] invMass_var = ${invMass_var}"; shift;;
	--scenario) scenario=$2; echo "[OPTION] scenario = ${scenario}"; shift;;
	--index) index=$2; shift;;
	--runRangesFile) runRangesFile=$2; echo "[OPTION] runRangesFile = ${runRangesFile}"; shift;; 
	--selection) selection=$2; echo "[OPTION] selection = ${selection}"; shift;;
	--baseDir) baseDir=$2; echo "[OPTION] baseDir = $baseDir"; shift;;
	--commonCut) commonCut=$2; echo "[OPTION] commonCut = $commonCut"; shift;;
	(--) shift; break;;
	(-*) usage; echo "$0: error - unrecognized option $1" 1>&2; usage >> /dev/stderr; exit 1;;
	(*) break;;
    esac
    shift
done

file=`basename ${configFile} .dat`

if [ ! -d "tmp" ];then mkdir tmp; fi

# file with ntuples
if [ -z "${configFile}" ];then
    echo "[ERROR] configFile not specified" >> /dev/stderr
    exit 1
fi

#case ${selection} in
#    WP80PU)
#        ;;
#    WP90PU)
#	;;
#    loose)
#	;;
#    medium)
#	;;
#    tight)
#	;;
#    diphotonIso25nsRun2Boff)
#	;;
#    noID) #To be used in rare cases ;-)
#	;;
#    *)
#	echo "[ERROR] Selection ${selection} not configured" >> /dev/stderr
#        exit 1
#        ;;
#esac



case ${STEP} in
    help) 
	echo "List of steps:"
	echo "   - 1: stability vs time (runNumber x eta)"
	echo "   - 2: eta x R9 (smearing method)"
#	echo "   - 2fit: eta x R9 (fit method)"
#	echo "   - 3: closure test of 2: eta x R9 (fit method)"
	echo "   - 4: closure test of 2: eta x R9 (smearing method), for final histograms and profiles"
	echo "   - 5: plot and profile only in Et categories with smearings from step4"
	echo "   - 6: smearings from step4, scales in Et x eta x R9 categories"
	echo "   - 7: eta x R9 x Et, scale and smearings"
	echo "   - 8: eta x R9 x Et, scale and smearings applied: closure test"
	;;
    0)	STEP0=y;;
    1)	STEP1=y;;
    1stability) STEP1Stability=y;;
    1plotter) STEP1Plotter=y;;
    2) 	STEP2=y;;
    3) 	STEP3=y;; 
    4)  STEP4=y;;
    4bis)  STEP4bis=y; extension=amctnlo;;
    # for systematics
    4amctnlo) STEP4=y; extension=amctnlo;; 
    4madgraph) STEP4=y; extension=madgraph;; 
    4weight) STEP4=y; extension=weight;;
    4medium) STEP4=y; extension=medium;;
    4tight)  STEP4=y; extension=tight;;
    4Et_22)  STEP4=y; extension=Et_22;;
    4Et_25)  STEP4=y; extension=Et_25;;
    4R9_93)  STEP4=y; extension=r9syst_93;;
    4R9_95)  STEP4=y; extension=r9syst_95;;
    4pho)    STEP4=y; extension=invMass_SC_pho_regrCorr;;
    4ele)    STEP4=y; extension=invMass_SC_corr;;
    5)  STEP5=y;;
    6)  STEP6=y;;
    8)  STEP8=y;;
    9)  STEP9=y;;
    9nPV)  STEP9=y; extension=nPV;;
    10)  STEP10=y;;
    12)  STEP12=y;;
#     madgraph) MCSAMPLE=y; extension=madgraph;; 
#     powheg) MCSAMPLE=y; extension=powheg;; 
#     sherpa) MCSAMPLE=y; extension=sherpa;; 
#    madgraph) PDFWEIGHT=y; extension=madgraph;; 
#    powheg)   PDFWEIGHT=y; extension=powheg;; 
#    sherpa)   PDFWEIGHT=y; extension=sherpa;; 
    pdfWeight) PDFWEIGHT=y;  extension=pdfWeight;;
    fsrWeight) PDFWEIGHT=y;  extension=fsrWeight;;
    weakWeight) PDFWEIGHT=y;  extension=weakWeight;;
    pdfWeightZPt) PDFWEIGHT=y;     extension=pdfWeightZPt;;
    gainSwitch) GAINSWITCH=y; extension=gainSwitch;echo "[OPTION] $extension";;
    gainSwitch2) GAINSWITCH=y; extension=gainSwitch2;echo "[OPTION] $extension";;
    gainSwitch3) GAINSWITCH=y; extension=gainSwitch3;echo "[OPTION] $extension";;
    gainSwitch4) GAINSWITCH=y; extension=gainSwitch4;echo "[OPTION] $extension";;
    gainSwitch5) GAINSWITCH=y; extension=gainSwitch5;echo "[OPTION] $extension";;
    gainSwitch6) GAINSWITCH=y; extension=gainSwitch6;echo "[OPTION] $extension";;
    gainSwitch7) GAINSWITCH=y; extension=gainSwitch7;echo "[OPTION] $extension";;
    gainSwitchEne) GAINSWITCH=y; extension=gainSwitchEne;echo "[OPTION] $extension";;
    gainSwitchSeedEne) GAINSWITCH=y; extension=gainSwitchSeedEne;echo "[OPTION] $extension";;
    7)  STEP7=y;;
    1-2) STEP1=y; STEP2=y;;
    1-3) STEP1=y; STEP2=y; STEP3=y; SLIDE=y;;
    2-3) STEP2=y; STEP3=y; SLIDE=y;;
    1-3stability) STEP1=y; STEP2=y; STEP3=y; STEP3Stability=y; SLIDE=y;;
    2-3stability) STEP2=y; STEP3=y; STEP3Stability=y; SLIDE=y;;
    slide) SLIDE=y;;
    3stability) STEP3Stability=y;;
	syst) SYSTEMATICS=y;;
    3weight) STEP3WEIGHT=y; STEP3=y; extension=weight;;
    2fit) STEP2FIT=y;;
    1-2fit) STEP1=y; STEP2FIT=y;;
    all) STEP1stability=y; STEP1=y; STEP2FIT=y; STEP3=y; STEP3Stability=y; STEP4=y;; # SLIDE=y;;
    *)
	echo "[ERROR] Step ${STEP} not implemented" >> /dev/stderr
	exit 1
	;;
esac




#####################
outFileStep1=step1-${invMass_var}-${selection}-${commonCut}-Eta_scales.dat
#outFileStep2=step2${extension}-${invMass_var}-${selection}-${commonCut}-EtaR9.dat
outFileStep2=step2-${invMass_var}-${selection}-${commonCut}-EtaR9_scales.dat
outFileStep4=step4-${invMass_var}-${newSelection}-${commonCut}-EtaR9.dat
outFileStep7=step7-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat
outFileStep8=step8-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat
outFileStep9=step9-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat
outFileStep12=step12-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat

# Make sure that the files indicated in outFileList are in the same order of stepNameList
outFileList=(
"${outFileStep1}"
"${outFileStep2}"
"${outFileStep4}"
"step8-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat"
"step9gainSwitch-${invMass_var}-${selection}-${commonCut}-EtaR9Et.dat"
"${outFileStep9}"
"${outFileStep12}"
)
stepNameList=(
"step1"
"step2"
"step4"
"step8"
"step9gainSwitch"
"step9"
"step12"
)

if [ -n "${STEP0}" ];then
    echo "This is step0, just to check the dir name"
    mcName ${configFile}
    echo "mcName: ${mcName}"
    mcName=$(echo $mcName | sed 's|.root||g')

    if [ "${invMass_var}" == "invMass_regrCorr_egamma" ];then
	outDirMC=$baseDir/MCodd/${mcName}/${selection}/${invMass_var}
	outDirMC_eos=$eosDir/MCodd/${mcName}/${selection}/${invMass_var}
	isOdd="--isOddMC"
    else
	outDirMC=$baseDir/MC/${mcName}/${selection}/${invMass_var}
	outDirMC_eos=$eosDir/MC/${mcName}/${selection}/${invMass_var}
    fi
    echo "outDirMC is ${outDirMC}"
    outDirData=$baseDir/dato/`basename ${configFile} .dat`/${selection}/${invMass_var}
    outDirData_eos=$eosDir/dato/`basename ${configFile} .dat`/${selection}/${invMass_var}
    outDirTable=${outDirData}/table
    outDirTable_eos=${outDirData_eos}/table
fi

if [ "${invMass_var}" == "invMass_regrCorr_egamma" ];then
    outDirMC=$baseDir/MCodd/${mcName}/${selection}/${invMass_var}
    outDirMC_eos=$eosDir/MCodd/${mcName}/${selection}/${invMass_var}
    isOdd="--isOddMC"
else
    outDirMC=$baseDir/MC/${mcName}/${selection}/${invMass_var}
    outDirMC_eos=$eosDir/MC/${mcName}/${selection}/${invMass_var}
fi
echo "outDirMC is ${outDirMC}"
outDirData=$baseDir/dato/`basename ${configFile} .dat`/${selection}/${invMass_var}
outDirData_eos=$eosDir/dato/`basename ${configFile} .dat`/${selection}/${invMass_var}
outDirTable=${outDirData}/table
outDirTable_eos=${outDirData_eos}/table


if [ -n "${STEP2}" ];then
    #eta x R9 with smearing method on top of step1

    if [ "${extension}" == "medium" -o "${extension}" == "tight" ];then
	newSelection=${extension}
    else
	newSelection=${selection}
    fi
    if [ ! -e "${outDirMC}/fitres" ];then mkdir ${outDirMC}/fitres -p; fi
    if [ ! -e "${outDirMC}/img" ];then mkdir ${outDirMC}/img -p; fi
    if [ ! -e "${outDirData}/step2${extension}/fitres" ];then mkdir ${outDirData}/step2${extension}/fitres -p; fi
    if [ ! -e "${outDirData}/step2${extension}/img" ];then mkdir ${outDirData}/step2${extension}/img -p; fi

    regionFileEB=${regionFileStep2EB}
    regionFileEE=${regionFileStep2EE}
    basenameEB=`basename $regionFileEB .dat`
    basenameEE=`basename $regionFileEE .dat`
    regionFile=$regionFileEB
    outFile=outFile-`basename ${outFileStep2} _scales.dat`.dat
    #outFileStep2 has only the scales inside, outFile has both scales and smearings

    #categorize in eta-r9(prime) regions

    if [[ $scenario = "Categorize" ]] || [[ $scenario = "" ]]; then
	
        
	touch ${outDirData}/step2${extension}/`basename ${configFile}` #It seems better to touch first, if you write on eos
	cat $configFile > ${outDirData}/step2${extension}/`basename ${configFile}`
	for tag in `grep "^d" $configFile | grep treeProducerWMassEle | awk -F" " ' { print $1 } '`
	do
	    echo "[STATUS] Writing scale root files produced in step1 for " $tag
	    echo "${tag} scaleEle_Eta ${outDirData}/step1/scaleEle_Eta_${tag}-`basename $configFile .dat`.root" >> ${outDirData}/step2${extension}/`basename ${configFile}`
	done

	mkSmearerCatSignal $regionFileEB $outDirData/step2${extension}/`basename $configFile`
	mkSmearerCatSignal $regionFileEE $outDirData/step2${extension}/`basename $configFile`
	mkSmearerCatData   $regionFileEB $outDirData/step2${extension}/`basename $configFile`
	mkSmearerCatData   $regionFileEE $outDirData/step2${extension}/`basename $configFile`
	
    fi

    if [[ $scenario = "Test_job" ]]; then
	if [ ! -e "${outDirTable}/${outFile}" -o -n "$FORCE" ];then
	    if [ ! -e "${outDirData}/step2${extension}/fitres_test" ];then mkdir ${outDirData}/step2${extension}/fitres_test -p; fi
	    echo "You are sending a test job to see if you need to pass an initFile"	
	fi

	./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --smearerFit --autoNsmear --autoBin --onlyScale --outDirFitResData=${outDirData}/step2${extension}/fitres_test
	
	./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --smearerFit --autoNsmear --autoBin --onlyScale --outDirFitResData=${outDirData}/step2${extension}/fitres_test

	echo "Now you should decide if you want to copy and edit"
	echo "cp ${outDirData}/step2${extension}/fitres_test/params-${basenameEB}-${commonCut}.txt ${outDirData}/step2${extension}/fitres/init-params-step2_1-${commonCut}.txt"
	echo "Now you should decide if you want to copy and edit"
	echo "cp ${outDirData}/step2${extension}/fitres_test/params-${basenameEE}-${commonCut}.txt ${outDirData}/step2${extension}/fitres/init-params-step2_2-${commonCut}.txt"
    fi
    
    if [[ $scenario = "Submit_jobs" ]] || [[ $scenario = "" ]]; then
	echo "Jobs are submitted only if this file doesn't exist: ${outDirTable}/${outFile}"
	
	if [ ! -e "${outDirTable}/${outFile}" -o -n "$FORCE" ];then
	    
	    if [ ! -e "${outDirMC}/fitres" ];then mkdir ${outDirMC}/fitres -p; fi
	    if [ ! -e "${outDirMC}/img" ];then mkdir ${outDirMC}/img -p; fi
	    if [ ! -e "${outDirData}/step2${extension}/fitres" ];then mkdir ${outDirData}/step2${extension}/fitres -p; fi
	    if [ ! -e "${outDirData}/step2${extension}/img" ];then mkdir ${outDirData}/step2${extension}/img -p; fi  
	fi
	
	#In general I didn't need any initFile for step2
	initFile_1=""
	initFile_2=""

	for index in `seq 1 50`
	do
	    mkdir ${outDirData}/step2/${index}/fitres/ -p 
	    mkdir ${outDirData}/step2/${index}/img -p 
	done
	bsub -q cmscaf1nd \
	    -o ${outDirData}/step2/%I/fitres/`basename ${outFile} .dat`-${basenameEB}.log \
	    -J "${basenameEB} step2[1-50]" \
	    "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; mkdir ${outDirData}/step2/\$LSB_JOBINDEX/fitres/ -p; mkdir ${outDirData}/step2/\$LSB_JOBINDEX/img -p; ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --outDirFitResMC=${outDirMC}/fitres --outDirImgMC=${outDirMC}/img --outDirImgData=${outDirData}/step2/\$LSB_JOBINDEX/img/ --outDirFitResData=${outDirData}/step2/\$LSB_JOBINDEX/fitres  --smearerFit --onlyScale --autoNsmear --autoBin ${initFile_1} || exit 1; touch ${outDirData}/step2/\$LSB_JOBINDEX/${basenameEB}-done"

	bsub -q cmscaf1nd \
	    -o ${outDirData}/step2/%I/fitres/`basename ${outFile} .dat`-${basenameEE}.log \
	    -J "${basenameEE} step2[1-50]" \
	    "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; mkdir ${outDirData}/step2/\$LSB_JOBINDEX/fitres/ -p; mkdir ${outDirData}/step2/\$LSB_JOBINDEX/img -p; ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --outDirFitResMC=${outDirMC}/fitres --outDirImgMC=${outDirMC}/img --outDirImgData=${outDirData}/step2/\$LSB_JOBINDEX/img/ --outDirFitResData=${outDirData}/step2/\$LSB_JOBINDEX/fitres  --smearerFit --onlyScale --autoNsmear --autoBin ${initFile_2} || exit 1; touch ${outDirData}/step2/\$LSB_JOBINDEX/${basenameEE}-done"

    fi #Submit_jobs

    if [[ $scenario = Fit_Likelihood_1 ]] || [[ $scenario = "" ]]; then
	./script/haddTGraph.sh -o ${outDirData}/step2/fitres/outProfile-${basenameEB}-${commonCut}.root ${outDirData}/step2/*/fitres/outProfile-${basenameEB}-${commonCut}.root
	./script/haddTGraph.sh -o ${outDirData}/step2/fitres/outProfile-${basenameEE}-${commonCut}.root ${outDirData}/step2/*/fitres/outProfile-${basenameEE}-${commonCut}.root
	
	######################################################33
	echo "{" > tmp/fitProfiles.C
	echo "gROOT->ProcessLine(\".include $ROOFITSYS/include\");" >> tmp/fitProfiles.C
	echo "gROOT->ProcessLine(\".L macro/macro_fit.C+\");" >> tmp/fitProfiles.C
	echo "gROOT->ProcessLine(\".L macro/plot_data_mc.C+\");" >> tmp/fitProfiles.C
	echo "FitProfile2(\"${outDirData}/step2/fitres/outProfile-${basenameEB}-${commonCut}.root\",\"\",\"\",true,true,false);" >> tmp/fitProfiles.C
	echo "FitProfile2(\"${outDirData}/step2/fitres/outProfile-${basenameEE}-${commonCut}.root\",\"\",\"\",true,true,false);" >> tmp/fitProfiles.C
	echo "}" >> tmp/fitProfiles.C
	root -l -b -q tmp/fitProfiles.C
	

        mkdir -p  ${outDirTable}
	cat ${outDirData}/step2${extension}/img/outProfile-${basenameEB}-${commonCut}-FitResult-.config > ${outDirTable}/${outFile}
	grep -v absEta_0_1 ${outDirData}/step2${extension}/img/outProfile-${basenameEE}-${commonCut}-FitResult-.config >> ${outDirTable}/${outFile}
	echo "results (scale and smearings) for step2 are in ${outDirTable}/${outFile}"
#	cat "`echo $initFile | sed 's|.*=||'`" |grep "C L" >>  ${outDirTable}/${outFile}
    fi

    if [[ $scenario = Plot_after_fit ]] || [[ $scenario = "" ]]; then
	echo "[STATUS] Plotting Data/MC using as initFile the final results (just to have the proper data/MC plots)"
        ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --selection=${newSelection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step2${extension}/img/ --outDirFitResData=${outDirData}/step2${extension}/fitres --constTermFix  --smearerFit  --smearingEt --autoNsmear --onlyScale --autoBin --initFile=${outDirTable}/${outFile} --plotOnly  || exit 1
        ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --selection=${selection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step2${extension}/img/ --outDirFitResData=${outDirData}/step2${extension}/fitres  --smearerFit --autoNsmear --onlyScale --autoBin --initFile=${outDirTable}/${outFile} --plotOnly  || exit 1
    fi #Fit_and_plot

    #### Setting file with scale corrections of step1 x step2
    if [[ $scenario = write_down_corr_step1_step2 ]] || [[ $scenario = "" ]] || [[ $scenario = Fit_Likelihood ]]; then
	###scale
	grep scale ${outDirData}/step2/img/outProfile-${basenameEB}-${commonCut}-FitResult-.config |  sed -r 's|[ ]+|\t|g;' | cut -f 1,3,5 | sed "s|scale_||;s|-${commonCut}||" | sed 's|\([^ \t]*\)-gold|\1 gold|; s|\([^ \t]*\)-bad|\1 bad|;s|\([^ \t]*\)-highR9|\1 highR9|; s|\([^ \t]*\)-lowR9|\1 lowR9|'  > tmp/res_corr.dat 
        grep scale ${outDirData}/step2/img/outProfile-${basenameEE}-${commonCut}-FitResult-.config | grep -v absEta_0_1| sed -r 's|[ ]+|\t|g;' | cut -f 1,3,5 | sed "s|scale_||;s|-${commonCut}||" | sed 's|\([^ \t]*\)-gold|\1 gold|; s|\([^ \t]*\)-bad|\1 bad|;s|\([^ \t]*\)-highR9|\1 highR9|; s|\([^ \t]*\)-lowR9|\1 lowR9|'  >> tmp/res_corr.dat 
	
	awk -f awk/prodScaleCorrSteps.awk tmp/res_corr.dat awk/dummyStep1Result.txt > ${outDirTable}/${outFileStep2}

	echo "complete set of corrections for scale step1*step2 is in ${outDirTable}/${outFileStep2}"
    fi
    if [[ $scenario = root_corr_step1_step2 ]] || [[ $scenario = "" ]] || [[ $scenario = Fit_Likelihood ]]; then
	#save root files with step1*step2 (scale corrections)
	./bin/ZFitter.exe -f ${configFile} --regionsFile ${regionFile} --saveRootMacro --onlyScale --corrEleType EtaR9 --corrEleFile ${outDirTable}/${outFileStep2/_scales.dat/}|| exit 1
	#./bin/ZFitter.exe -f ${configFile} --regionsFile ${regionFile} --saveRootMacro --smearEleType stochastic --smearEleFile ${outDirTable}/smearing_corrections.dat || exit 1
	mv tmp/scaleEle_EtaR9_[s,d][1-9]-`basename $configFile .dat`.root ${outDirData}/step2/    
    fi

#################copying the dat file over the eos web space#######################
    if [[ $scenario = finalize_step2 ]] || [[ $scenario = "" ]] || [[ $scenario = Fit_Likelihood ]]; then
	if [ ! -d $printDir ];then 
            mkdir -p ${printDir}/DataMC
            cp /afs/cern.ch/user/g/gpetrucc/php/index.php ${printDir}
	fi
	if [ ! -d $printDir/DataMC ];then 
            mkdir -p ${printDir}/DataMC
            cp /afs/cern.ch/user/g/gpetrucc/php/index.php ${printDir}/DataMC
        fi
	mv test/dato/${file}/${selection}/${invMass_var}/step2/img/outProfile-scaleStep2smearing_*.png ${printDir}
	cp ${outDirTable}/${outFileStep2} ${printDir} #do not mv this file: otherwise "checkStepDep step2" will fail!
	./script/latex_table_writer.sh ${outDirTable}/${outFile} -${commonCut}
	echo table_`basename ${outFile} .dat`"_scale_tex.dat"
	cp tmp/table_`basename ${outFile} .dat`_scale_tex.dat ${printDir}
	cp tmp/table_`basename ${outFile} .dat`_smear_tex.dat ${printDir}

        ###Data_MC_plots at the end of step2 (here the MC is shifted (not the data as in the analyses) to see the original position of the data peak)
	file2EB=`basename ${regionFileEB} .dat`
	file2EE=`basename ${regionFileEE} .dat`
#
	./script/plot_histos_validation.sh test/dato/${file}/${selection}/${invMass_var}/step2${extension}/fitres/histos-${file2EB}-${commonCut}.root
	./script/plot_histos_validation.sh test/dato/${file}/${selection}/${invMass_var}/step2${extension}/fitres/histos-${file2EE}-${commonCut}.root
	cp test/dato/${file}/${selection}/${invMass_var}/step2${extension}/img/histos-* ${printDir}/DataMC/
    fi
#Here step2 is closed	
fi

#######Et steps begin!#######
if [ -n "${STEP5}" ];then
    echo "You are making step5"
    if [ "${extension}" == "medium" -o "${extension}" == "tight" ];then
	newSelection=${extension}
    else
	newSelection=${selection}
    fi
    #eta x Et with smearing method (use step4 as initialization)
    regionFileEB=${regionFileStep5EB}
    regionFileEE=${regionFileStep5EE}
    basenameEB=`basename $regionFileEB .dat`
    basenameEE=`basename $regionFileEE .dat`
    outFile=${outDirTable}/step5${extension}-${invMass_var}-${newSelection}-${commonCut}.dat

    if [ ! -e "${outDirTable}" ];then mkdir ${outDirTable} -p; fi

    echo "Categorization and job submission in step5 is done if ${outFile} does NOT exist"
    if [ ! -e "${outFile}" ];then

	if [ ! -e "${outDirMC}/${extension}/fitres" ];then mkdir ${outDirMC}/${extension}/fitres -p; fi
	if [ ! -e "${outDirMC}/${extension}/img" ];then    mkdir ${outDirMC}/${extension}/img -p; fi
	if [ ! -e "${outDirData}/step5${extension}/fitres" ];then mkdir ${outDirData}/step5${extension}/fitres -p; fi
	if [ ! -e "${outDirData}/step5${extension}/img" ];then    mkdir ${outDirData}/step5${extension}/img -p; fi
	
	
	if [ "${extension}" == "weight" ];then
	    updateOnly="$updateOnly --useR9weight"
	fi

	#Categorize in Et X Eta
	echo "configFile for step5 is " ${configFile}
	if [[ $scenario == "Categorize" ]]; then
	    ./bin/ZFitter.exe -f ${configFile} --regionsFile ${regionFileEB} --saveRootMacro --addBranch=smearerCat  --smearerFit
	    ./bin/ZFitter.exe -f ${configFile} --regionsFile ${regionFileEE} --saveRootMacro --addBranch=smearerCat  --smearerFit

	    tags=`grep -v '#' $configFile | sed -r 's|[ ]+|\t|g; s|[\t]+|\t|g' | cut -f 1  | sort | uniq | grep [s,d][1-9]`
            exit
	    baseName=`basename $regionFileEB .dat`
	    echo ${baseName}
	    for tag in $tags
	    do
	        if [ "`grep -v '#' $configFile | grep \"^$tag\" | cut -f 2 | grep -c smearerCat_${baseName}`" == "0" ];then
                    
	            mv tmp/smearerCat_`basename $regionFileEB .dat`_${tag}-`basename $configFile .dat`.root data/smearerCat/smearerCat_`basename $regionFileEB .dat`_${tag}-`basename $configFile .dat`.root || exit 1
	            echo -e "$tag\tsmearerCat_`basename $regionFileEB .dat`\tdata/smearerCat/smearerCat_`basename $regionFileEB .dat`_${tag}-`basename $configFile .dat`.root" >> $configFile
	            mv tmp/smearerCat_`basename $regionFileEE .dat`_${tag}-`basename $configFile .dat`.root data/smearerCat/smearerCat_`basename $regionFileEE .dat`_${tag}-`basename $configFile .dat`.root || exit 1
                    
	            echo -e "$tag\tsmearerCat_`basename $regionFileEE .dat`\tdata/smearerCat/smearerCat_`basename $regionFileEE .dat`_${tag}-`basename $configFile .dat`.root" >> $configFile
                    
	        fi
	    done
	fi #closes categorization in Et X eta X R9 categories

	# root files for corrections have already been created in step2 and checked in step2
	# include them in the dat file for step5
	echo "config File used for jobs in step5 is " $configFile
        echo "fit results will be in ${outDirData}/step5${extension}"
        
	for index in `seq 1 50`
	do
	    mkdir ${outDirData}/step5${extension}/${index}/fitres/ -p 
	    mkdir ${outDirData}/step5${extension}/${index}/img -p 
	done

        if [[ $scenario = "Test_job" ]]; then

	    mkdir ${outDirData}/step5${extension}/fitres_test -p 

	    ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --smearerFit --autoNsmear --autoBin --onlyScale --outDirFitResData=${outDirData}/step5${extension}/fitres_test
	    
	    ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --invMass_var ${invMass_var} --commonCut ${commonCut} --selection=${selection} --smearerFit --autoNsmear --autoBin --onlyScale --outDirFitResData=${outDirData}/step5${extension}/fitres_test
            exit
        fi


        if [[ $scenario = "Submit_jobs" ]]; then

## --profileOnly --plotOnly
	    bsub -q cmscaf1nd \
	        -o ${outDirData}/step5${extension}/%I/fitres/`basename ${outFile} .dat`-${basenameEB}-stdout.log \
	        -J "${basenameEB} step5${extension}[1-50]" \
	        "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; 
./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --selection=${selection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step5${extension}/\$LSB_JOBINDEX/img/ --outDirFitResData=${outDirData}/step5${extension}/\$LSB_JOBINDEX/fitres --smearerFit --autoNsmear --autoBin --onlyScale  --profileOnly --plotOnly || exit 1; touch ${outDirData}/step5${extension}/\$LSB_JOBINDEX/`basename $regionFileEB .dat`-done"

	    bsub -q cmscaf1nd \
	        -o ${outDirData}/step5${extension}/%I/fitres/`basename ${outFile} .dat`-${basenameEE}-stdout.log \
	        -J "${basenameEE} step5${extension}[1-50]" \
	        "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; 
./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --selection=${selection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step5${extension}/\$LSB_JOBINDEX/img/ --outDirFitResData=${outDirData}/step5${extension}/\$LSB_JOBINDEX/fitres  --smearerFit --autoNsmear --autoBin --onlyScale  --profileOnly --plotOnly || exit 1; touch ${outDirData}/step5${extension}/\$LSB_JOBINDEX/`basename $regionFileEE .dat`-done"

            exit
        fi

        if [[ $scenario = Fit_Likelihood_1 ]]; then
            
            ./script/haddTGraph.sh -o ${outDirData}/step5${extension}/fitres/outProfile-$basenameEB-${commonCut}.root ${outDirData}/step5${extension}/*/fitres/outProfile-$basenameEB-${commonCut}.root
            ./script/haddTGraph.sh -o ${outDirData}/step5${extension}/fitres/outProfile-$basenameEE-${commonCut}.root ${outDirData}/step5${extension}/*/fitres/outProfile-$basenameEE-${commonCut}.root
            
	######################################################33
            echo "{" > tmp/fitProfiles.C
            echo "gROOT->ProcessLine(\".include $ROOFITSYS/include\");" >> tmp/fitProfiles.C
            echo "gROOT->ProcessLine(\".L macro/macro_fit.C+\");" >> tmp/fitProfiles.C
            echo "gROOT->ProcessLine(\".L macro/plot_data_mc.C+\");" >> tmp/fitProfiles.C
            echo "FitProfile2(\"${outDirData}/step5${extension}/fitres/outProfile-$basenameEB-${commonCut}.root\",\"\",\"\",true,true,false);" >> tmp/fitProfiles.C
            echo "FitProfile2(\"${outDirData}/step5${extension}/fitres/outProfile-$basenameEE-${commonCut}.root\",\"\",\"\",true,true,false);" >> tmp/fitProfiles.C
            echo "}" >> tmp/fitProfiles.C
            root -l -b -q tmp/fitProfiles.C

            echo "InitFile is "${initFile}
            echo "Fit results of step5 in " ${outDirData}/step5${extension}/img/outProfile-${basenameEB}-${commonCut}-FitResult-.config
            cat ${outDirData}/step5${extension}/img/outProfile-${basenameEB}-${commonCut}-FitResult-.config > ${outFile}
            cat ${outDirData}/step5${extension}/img/outProfile-${basenameEE}-${commonCut}-FitResult-.config >> ${outFile}

            cat "`echo $initFile | sed 's|.*=||'`" |grep "C L" >>  ${outFile}
            echo "outFile of step5 is " ${outFile} " (this will be the initFile of the plots)"
            exit
        fi

    fi #all of this is done only if ${outFile} doesn't exist

    if [[ $scenario = Plot_after_fit ]]; then
        # bsub -q cmscaf1nd \
        #     -o tmp/`basename ${outFile} .dat`-${basenameEB}-stdout.log \
        #     -J "plotEB" \
        #     "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; 
        ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEB} $isOdd $updateOnly --selection=${selection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step5${extension}/img/ --outDirFitResData=${outDirData}/step5${extension}/fitres --smearerFit --autoNsmear --autoBin --onlyScale --initFile=${outFile}  --plotOnly  || exit 1
        # bsub -q cmscaf1nd \
        #     -o tmp/`basename ${outFile} .dat`-${basenameEE}-stdout.log \
        #     -J "plotEE" \
        #     "cd $PWD; eval \`scramv1 runtime -sh\`; uname -a;  echo \$CMSSW_VERSION; 
        ./bin/ZFitter.exe -f $configFile --regionsFile ${regionFileEE} $isOdd $updateOnly --selection=${selection}  --invMass_var ${invMass_var} --commonCut ${commonCut} --outDirFitResMC=${outDirMC}/${extension}/fitres --outDirImgMC=${outDirMC}/${extension}/img --outDirImgData=${outDirData}/step5${extension}/img/ --outDirFitResData=${outDirData}/step5${extension}/fitres --smearerFit --autoNsmear --autoBin --onlyScale --initFile=${outFile}  --plotOnly  || exit 1 
        exit
    fi

    if [[ $scenario = "write_corr" ]]; then
	grep scale ${outFile} | sed -r 's|[ ]+|\t|g;' | cut -f 1,3,5 | sed "s|scale_||;s|-${commonCut}||" | sed 's|\(Et_[0-9]*_[0-9]*\)-\([^ \t]*\)|\2 \1 |' > tmp/res_corr_step5.dat
	outFileStep5=${outDirTable}/scale_step5${extension}-${invMass_var}-${newSelection}-${commonCut}-EtaR9Et.dat
        outFileStep2="/afs/cern.ch/work/e/emanuele/wmass/heppy/CMSSW_8_0_25/src/ECALELF/ZFitter/test/dato/wmass-22Jan2012-stdMC/LooseEleID/mZ1/table/step2-mZ1-LooseEleID-EtSingleEle_25-EtaR9_scales.dat"
        echo "Multiplying corrections from EtaR9 (step2): ${outFileStep2} x Et-dependent ones: tmp/res_corr_step5.dat" 
	awk -f awk/prodScaleCorrSteps.awk tmp/res_corr_step5.dat ${outFileStep2} > ${outFileStep5}
        echo "===> Corrections file in: ${outFileStep5}"
    fi

    if [[ $scenario = "finalize_step5" ]] || [[ $scenario = "" ]]; then
	if [ ! -d $printDir/step5 ];then 
            mkdir -p ${printDir}/step5/DataMC
            cp /afs/cern.ch/user/g/gpetrucc/php/index.php ${printDir}/step5/DataMC
	fi

	cp test/dato/${file}/${selection}/${invMass_var}/step5/img/outProfile-*.png ${printDir}/step5
	./script/plot_histos_validation.sh test/dato/${file}/${selection}/${invMass_var}/step5/fitres/histos-${basenameEB}-${commonCut}.root
	./script/plot_histos_validation.sh test/dato/${file}/${selection}/${invMass_var}/step5/fitres/histos-${basenameEE}-${commonCut}.root
	cp test/dato/${file}/${selection}/${invMass_var}/step5/img/histos-* ${printDir}/step5/DataMC
    fi

fi

