set ylabel "P(x)"
set xlabel "x"
set xrange [0:15]
set yrange [0:0.3]
set border 1+2
set xtics nomirror
set ytics nomirror
# gnuplot doesn't have a built-in combinations or "n choose r" function, so we
# implement it.  gnuplot does have a factorial function, but to avoid overflow
# problems, it's safer to use the fact that gamma(n+1) = n! and do the
# calculation with logs.
choose(n,r) = exp(lgamma(n+1) - lgamma(r+1) - lgamma(n-r+1))
round(x) = floor(x+0.5)
negbinomial(x,s,p) = choose(s+round(x)-1,round(x)) * (p**s) * ((1-p)**round(x))
set label "s=3, p=0.6" at first 1.75,0.25 left tc lt 1
set label "s=5, p=0.5" at first 6,0.125 left tc lt 2
plot negbinomial(x,3.0,0.6) notitle w histeps lw 3,\
  negbinomial(x,5,0.5) notitle w histeps lw 3
