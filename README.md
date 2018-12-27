# AccuSim
Use the Simulator to calculate the data of the workload.


### 建置
建置請參考培堯的github https://github.com/baconYao/Accusim-Strace

1.	將asim/disksim內的.paths.in 檔修改路徑
2.	再依序將diskmodel、libparam、src內的.paths.in 和.paths 檔修改路徑
3.	Make libddbg

4.  修改util.c的line:240
		將default:加上break;
	修改util.c的line:828
		將go to next:和next拿掉
		再把if(...)移出第二層迴圈，且把!拿掉，完後後如下圖
5.	Make libparam

6. 	將asim/disksim/diskmodel/modules內的檔案權限全開
		輸入sudo chmod -f 777 *
	修改asim/disksim/diskmodel內的layout_g1.c
		line:212 加上break;
	在disksim/diskmodel/modules make，就會出現.c檔
	將diskmodel/modules內所有.c檔做縮排
	目的在於刪除 "%s" 與 " 之間的換行，避免fprintf的參數不完整，而造成Compile error
	將fprintf排好
7.	Make DiskModel

8.	cd asim/disksim/src/
		所有.c檔做縮排，同前一步驟做法
	修改src/disksim_disk.c
		line:1152(disk_get_number_of_blocks)和1156(disk_get_numcyls)的 static刪除
		line:1162(disk_get_mapping)的 static刪除並且縮排
	在disksim/src make即可
9.	Make Src
10.	Make Disksim
11.	Make Sim




### 執行  
./predict -a XXX.tr -s 256 -p 3 -x 0
(256=CacheSize)(3=LRU)(0表示不使用prefetch) 
