"""Deterministic Xcode project generation. Python 3 standard library only."""
from pathlib import Path
import hashlib,json

root=Path(__file__).resolve().parents[1]
objects={}
def uid(name):return hashlib.sha256(name.encode()).hexdigest()[:24].upper()
def put(name,value):
    key=uid(name);objects[key]=value;return key
def quoted(s):return json.dumps(s,ensure_ascii=True)
def encode(value):
    if isinstance(value,dict):return '{ '+ ' '.join(quoted(str(k))+' = '+encode(v)+';' for k,v in value.items())+' }'
    if isinstance(value,list):return '( '+', '.join(encode(v) for v in value)+' )'
    return quoted(str(value))

sources=[('Walker/'+p.name,'sourcecode.swift') for p in sorted((root/'Walker').glob('*.swift'))]
sources += [('Walker/Bridge.c','sourcecode.c.c'),('../shared/walkerfile.c','sourcecode.c.c'),('../shared/species.c','sourcecode.c.c')]
resources=[('Walker/Assets.xcassets','folder.assetcatalog'),('Walker/PrivacyInfo.xcprivacy','text.xml')]
resources += [('Walker/Resources/'+n+'.html','text.html') for n in ['privacy','credits','licence']]
refs=[];source_build=[];resource_build=[]
for path,kind in sources+resources+[('Walker/Bridge.h','sourcecode.c.h'),('Walker/Info.plist','text.plist.xml')]:
    ref=put('file:'+path,{'isa':'PBXFileReference','lastKnownFileType':kind,'path':path,'sourceTree':'<group>'});refs.append(ref)
    if (path,kind) in sources+resources:
        build=put('build:'+path,{'isa':'PBXBuildFile','fileRef':ref})
        (source_build if (path,kind) in sources else resource_build).append(build)
product=put('product',{'isa':'PBXFileReference','explicitFileType':'wrapper.application','includeInIndex':0,'path':'Pokewalker.app','sourceTree':'BUILT_PRODUCTS_DIR'})
products=put('products',{'isa':'PBXGroup','children':[product],'name':'Products','sourceTree':'<group>'})
group=put('main',{'isa':'PBXGroup','children':refs+[products],'sourceTree':'<group>'})
src=put('sources',{'isa':'PBXSourcesBuildPhase','buildActionMask':2147483647,'files':source_build,'runOnlyForDeploymentPostprocessing':0})
res=put('resources',{'isa':'PBXResourcesBuildPhase','buildActionMask':2147483647,'files':resource_build,'runOnlyForDeploymentPostprocessing':0})
framework=put('frameworks',{'isa':'PBXFrameworksBuildPhase','buildActionMask':2147483647,'files':[],'runOnlyForDeploymentPostprocessing':0})
project_configs=[];target_configs=[]
for name in ['Debug','Release']:
    common={'CLANG_ENABLE_MODULES':'YES','CLANG_ENABLE_OBJC_ARC':'YES','GCC_C_LANGUAGE_STANDARD':'c11','IPHONEOS_DEPLOYMENT_TARGET':'16.0','SDKROOT':'iphoneos','SWIFT_VERSION':'5.0','SWIFT_OPTIMIZATION_LEVEL':'-Onone' if name=='Debug' else '-O','DEBUG_INFORMATION_FORMAT':'dwarf' if name=='Debug' else 'dwarf-with-dsym','GCC_PREPROCESSOR_DEFINITIONS':['$(inherited)','DEBUG=1'] if name=='Debug' else ['$(inherited)']}
    project_configs.append(put('project:'+name,{'isa':'XCBuildConfiguration','buildSettings':common,'name':name}))
    settings={'PRODUCT_BUNDLE_IDENTIFIER':'io.github.sdtransporter.pokewalker.iphone','PRODUCT_NAME':'Pokewalker','INFOPLIST_FILE':'Walker/Info.plist','GENERATE_INFOPLIST_FILE':'NO','SWIFT_OBJC_BRIDGING_HEADER':'Walker/Bridge.h','HEADER_SEARCH_PATHS':['$(inherited)','$(SRCROOT)/../shared'],'ASSETCATALOG_COMPILER_APPICON_NAME':'AppIcon','TARGETED_DEVICE_FAMILY':'1','SUPPORTED_PLATFORMS':'iphoneos iphonesimulator','CODE_SIGN_STYLE':'Automatic','LD_RUNPATH_SEARCH_PATHS':['$(inherited)','@executable_path/Frameworks'],'ENABLE_USER_SCRIPT_SANDBOXING':'YES'}
    target_configs.append(put('target:'+name,{'isa':'XCBuildConfiguration','buildSettings':settings,'name':name}))
project_list=put('project:list',{'isa':'XCConfigurationList','buildConfigurations':project_configs,'defaultConfigurationIsVisible':0,'defaultConfigurationName':'Release'})
target_list=put('target:list',{'isa':'XCConfigurationList','buildConfigurations':target_configs,'defaultConfigurationIsVisible':0,'defaultConfigurationName':'Release'})
target=put('target',{'isa':'PBXNativeTarget','buildConfigurationList':target_list,'buildPhases':[src,framework,res],'buildRules':[],'dependencies':[],'name':'Pokewalker','productName':'Pokewalker','productReference':product,'productType':'com.apple.product-type.application'})
project=put('project',{'isa':'PBXProject','attributes':{'BuildIndependentTargetsInParallel':'YES','LastUpgradeCheck':'1600'},'buildConfigurationList':project_list,'compatibilityVersion':'Xcode 14.0','developmentRegion':'en','knownRegions':['en','Base'],'mainGroup':group,'productRefGroup':products,'projectDirPath':'','projectRoot':'','targets':[target]})
directory=root/'Pokewalker.xcodeproj';directory.mkdir(exist_ok=True)
text='// !$*UTF8*$!\n'+encode({'archiveVersion':1,'classes':{},'objectVersion':56,'objects':objects,'rootObject':project})+'\n'
(directory/'project.pbxproj').write_text(text,encoding='utf-8',newline='\n')
scheme=directory/'xcshareddata/xcschemes';scheme.mkdir(parents=True,exist_ok=True)
(scheme/'Pokewalker.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="Pokewalker.app" BlueprintName="Pokewalker" ReferencedContainer="container:Pokewalker.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" shouldUseLaunchSchemeArgsEnv="YES"/>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="Pokewalker.app" BlueprintName="Pokewalker" ReferencedContainer="container:Pokewalker.xcodeproj"/></BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES"/>
<AnalyzeAction buildConfiguration="Debug"/>
<ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
''',encoding='utf-8',newline='\n')
print('Generated',directory)
