# AccuSim
Use the Simulator to calculate the data of the workload.

### 建置
建置請參考培堯的github https://github.com/baconYao/Accusim-Strace

### 執行  
./predict -a XXX.tr -s 256 -p 3 -x 0
(256=CacheSize)(3=LRU)(0表示不使用prefetch) 
