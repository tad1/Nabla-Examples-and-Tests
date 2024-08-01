import re
import os.path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--outputDir")
parser.add_argument("--name")
parser.add_argument("--logFile")

args = parser.parse_args()
outputDir = args.outputDir
name = args.name
# temporary: remove that next commit
logFilePath = args.logFile

outputPath = os.path.join(outputDir, "assetLoading.csv")
assetNameRE = '.*Asset: \"(.*)\"'
assetLoadTimeRE = '.*Asset load time: (.*) ms'


files = []
res = []

print(logFilePath)

with open(logFilePath, "r") as logFile:
    for line in logFile:
        ANGroup = re.search(assetNameRE, line)
        ALTGroup = re.search(assetLoadTimeRE, line)
        if(ANGroup):
            assetName = ANGroup.group(1)
            files.append(assetName)
        if(ALTGroup):
            loadTime = ALTGroup.group(1)
            res.append(loadTime)

print(files)
print(res)

append = True if os.path.exists(outputPath) else False

if(append):
    with open(outputPath, "a") as file:
        file.write(f"{", ".join([name] + res)}\n")
else:
    with open(outputPath, "w") as file:
        file.write(f"{", ".join(["name"]+files)}\n")
        file.write(f"{", ".join([name] + res)}\n")