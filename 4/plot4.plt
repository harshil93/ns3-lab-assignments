set terminal png
set output "plot4.png"
set title "Final throughput vs Delay of link2"
set xlabel "Delay of link2"
set ylabel "Final throughput"

set xrange [0:+110]
plot "-"  title "Link2" with linespoints
10 1.1346
20 0.779558
30 0.433088
40 0.895334
50 0.342182
60 0.331891
70 0.27529
80 0.258995
90 0.231552
100 0.21783
e
