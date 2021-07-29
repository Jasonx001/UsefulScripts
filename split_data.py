import os
import random

# This script can split your dataset as train--val--test subset

files_path = "G:\\VOC2012\\Annotations" #This is a annotation path of your dataset.
if not os.path.exists(files_path):
    print("文件夹不存在！")
    exit(1)
test_rate = 0.2 #the test_rate is the ratio of test set over your whole dataset.
val_rate = 0.2 #the val_rate is the ratio of val set over your trainval subset.

files_name = sorted([file.split(".")[0] for file in os.listdir(files_path)])
files_num = len(files_name)
test_index = random.sample(range(0, files_num), k=int(files_num*test_rate))
trainval_files = []
test_files = []
# The first loop split the dataset as test and trainval subset.
for index, file_name in enumerate(files_name):
    if index in test_index:
        test_files.append(file_name)
    else:
        trainval_files.append(file_name)

trainval_num = len(trainval_files)
val_index = random.sample(range(0, trainval_num), k = int(trainval_num * val_rate))
train_files = [];
val_files = [];
# the second loop split the trainval sebset as train and val subset.
# the number of files in val set accounts 0.2 of the trainval subset. 
for index, file_name in enumerate(trainval_files):
    if index in val_index:
        val_files.append(file_name)
    else:
        train_files.append(file_name)

try:
    trainval_f = open("trainval.txt", "x")
    train_f = open("train.txt", "x")
    eval_f = open("val.txt", "x")
    test_f = open("test.txt", "x")
    trainval_f.write("\n".join(trainval_files))
    train_f.write("\n".join(train_files))
    eval_f.write("\n".join(val_files))
    test_f.write("\n".join(test_files))
except FileExistsError as e:
    print(e)
    exit(1)



