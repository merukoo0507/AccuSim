# AccuSim
Use the Simulator to calculate the data of the workload.


### 建置
1.	將asim/disksim內的.paths.in 檔修改路徑
2.	再依序將diskmodel、libparam、src內的.paths.in 和.paths 檔修改路徑
3.	Make libddbg
4.  修改util.c的line:240
		將default:加上break;
	修改util.c的line:828
		將go to next:和next拿掉
		再把if(...)移出第二層迴圈，且把!拿掉，完後後如下圖

5.	Make libparam


Make Disksim
Make Sim


建置請參考培堯的github https://github.com/baconYao/Accusim-Strace


### 執行  
./predict -a XXX.tr -s 256 -p 3 -x 0
(256=CacheSize)(3=LRU)(0表示不使用prefetch) 
