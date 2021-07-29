import os
import random


files_path = "G:\\研究生\\数据集\\VOC2012\\Annotations" #windows下路径写法
if not os.path.exists(files_path):
    print("文件夹不存在！")
    exit(1)
test_rate = 0.2
val_rate = 0.2

files_name = sorted([file.split(".")[0] for file in os.listdir(files_path)])
files_num = len(files_name)
test_index = random.sample(range(0, files_num), k=int(files_num*test_rate))
trainval_files = []
test_files = []
for index, file_name in enumerate(files_name):
    if index in test_index:
        test_files.append(file_name)
    else:
        trainval_files.append(file_name)

trainval_num = len(trainval_files)
val_index = random.sample(range(0, trainval_num), k = int(trainval_num * val_rate))
train_files = [];
val_files = [];
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



