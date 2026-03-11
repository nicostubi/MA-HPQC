set title "Memory Access Latency"
set xlabel "Working set size (kbytes)"
set ylabel "access time (ns)"
set term pdf

set output 'ws-0-512MB.pdf'
set xrange [0:524288]
plot 'data' using 1:2 w l title "stride 64", \
     'data' using 1:3 w l title "stride 128", \
     'data' using 1:4 w l title "stride 256", \
     'data' using 1:5 w l title "stride 512", \
     'data' using 1:6 w l title "stride 1024", \
     'data' using 1:7 w l title "stride 2048", \
     'data' using 1:8 w l title "stride 4096", \
     'data' using 1:9 w l title "stride 8192"

set output 'ws-0-32MB.pdf'
set xrange [0:32768]
replot

set output 'ws-0-512kB.pdf'
set xrange [0:512]
replot
