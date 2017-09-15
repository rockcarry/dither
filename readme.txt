基于误差扩散算法的半色调色彩抖动处理程序

使用方法

dither 工具
-----------
dither test.bmp palette.pal

第一个参数 test.bmp 是要处理的图片
第二个参数是调色板数据文件名

图片必须为 24bit RGB bmp 图片
调色板数据为可选参数，如果不指定，则默认使用如下调色板

0   0   0
255 255 255

即黑白两色的调色板


palette 工具
------------
palette -g N
创建标准灰度调色板，N 为灰度位深度
 - create standard gray palette, N is the bits number for gray scale.

palette -c N
创建标准彩色调色板，N 颜色分量位深度

palette -p filename N
创建最佳匹配的彩色调色板，N 为最大的颜色数



程序使用到的算法

256 色最佳匹配调色板的八叉树算法，针对算法做了优化，是一个快速算法

基于误差扩散的半色调抖动算法



rockcarry
2017-9-15




