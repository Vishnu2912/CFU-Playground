# Implementations and corresponding results
Ticks corresponding to each layer can be found below for each of the implementations - 1 tick correspond to 1024 cycles<br />
Total number of cycles required after each optimization also can be noted below

1. initial - Naive Implementation

"Event","Tag","Ticks"<br />
0,CONV_2D,3230<br />
1,CONV_2D,8119<br />
2,MAX_POOL_2D,129<br />
3,RESHAPE,10<br />
4,FULLY_CONNECTED,153<br />
    12M (    11920584) cycles total<br />
OK   Golden tests passed

2. soft_opt - Software Optimizations

"Event","Tag","Ticks"<br />
0,CONV_2D,2403<br />
1,CONV_2D,7411<br />
2,MAX_POOL_2D,129<br />
3,RESHAPE,10<br />
4,FULLY_CONNECTED,153<br />
    10M (    10349506) cycles total<br />
OK   Golden tests passed

3. unroll - Loop Unrolled Implementation

"Event","Tag","Ticks"<br />
0,CONV_2D,1676<br />
1,CONV_2D,5608<br />
2,MAX_POOL_2D,128<br />
3,RESHAPE,11<br />
4,FULLY_CONNECTED,153<br /><br />
  7760k (     7760048) cycles total<br />
OK   Golden tests passed

4. simd - SIMD Implementation

"Event","Tag","Ticks"<br />
0,CONV_2D,1689<br />
1,CONV_2D,1602<br />
2,MAX_POOL_2D,128<br />
3,RESHAPE,11<br />
4,FULLY_CONNECTED,153<br />
  3669k (     3668930) cycles total<br />
OK   Golden tests passed

##### Further for each of the above versions a better Implementaion done by storing and using the distinct filter value

5. soft_opt_filt - Filter storage Implementation upon software optimized version

"Event","Tag","Ticks"<br />
0,CONV_2D,1188<br />
1,CONV_2D,5511<br />
2,MAX_POOL_2D,129<br />
3,RESHAPE,10<br />
4,FULLY_CONNECTED,152<br />
  7160k (     7160029) cycles total<br />
OK   Golden tests passed

6. unroll_filt - Filter storage Implementation upon Loop Unrolled version

"Event","Tag","Ticks"<br />
0,CONV_2D,1298<br />
1,CONV_2D,2689<br />
2,MAX_POOL_2D,128<br />
3,RESHAPE,10<br />
4,FULLY_CONNECTED,152<br />
  4382k (     4381951) cycles total<br />
OK   Golden tests passed

7. simd_filt - Filter storage Implementation upon SIMD version 

"Event","Tag","Ticks"<br />
0,CONV_2D,1299<br />
1,CONV_2D,1591<br />
2,MAX_POOL_2D,128<br />
3,RESHAPE,11<br />
4,FULLY_CONNECTED,153<br />
  3259k (     3258762) cycles total<br />
OK   Golden tests passed

