#!/usr/bin/env python3

num = 0
with open('./alexnet.labels','w') as file:
    file.write('{')
    with open('./imagenet_2012.txt','r') as f:
        while True:
            line=f.readline()
            print (line)
            if not line:
                break

            label_list = line.split(' ')
            #print ('a',label_list)
            label_list = label_list[1:]
            print ('a',label_list)
            if num !=0:
                file.write(',\n')
            line = str(num)+ ": '"+' '.join(label_list)
            line = line[:-1]+"'"
            print ('line',line)
            num += 1

            file.write(line)
    file.write('}')
