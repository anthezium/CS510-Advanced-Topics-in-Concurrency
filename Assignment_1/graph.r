library(ggplot2)
library(scales)

args                 <- commandArgs(TRUE)
name                 <- args[1]
#name                 <- "data/test_my_spinlock.csv"
d                    <- read.csv(file=name,head=TRUE,sep=",")
d$ticks_per_thread   <- d$ticks / d$nt
d$nt                 <- factor(d$nt)

#tpt_by_test_and_nt   <- with(d, aggregate(ticks_per_thread ~ test + nt, d, median))
ticks_by_test_and_nt <- with(d, aggregate(ticks ~ test + nt + critical_section_accesses, d, median))

n_breaks             <- 40

is_graphable <- function(s) { return (! grepl("_nograph", s)) }

gen_graph <- function(data, xc, yc, f, name, n_breaks, lbs){
  data <- subset(data, f(test))
  ggplot(data=data, aes_string(xc, yc, group="test")) + 
    geom_line (data=data, aes_string(colour="test")) + 
    geom_point(data=data, aes_string(shape="test",colour="test")) + 
    scale_y_continuous(breaks = pretty_breaks(n_breaks)) +
    lbs
    
  ggsave(paste0(name,".pdf"), width=16, height=9, units="in", limitsize=FALSE)
}

#gen_graph(subset(tpt_by_test_and_nt, critical_section_accesses == 0), "nt", "ticks_per_thread", is_graphable, paste0(name, "_tpt"), n_breaks,
#         labs(title = "Per-thread Throughput vs Number of Threads (no memory accesses during critical sections)",
#              x     = "Number of Threads",
#              y     = "Total duration / Number of Threads"))

gen_graph(subset(ticks_by_test_and_nt, critical_section_accesses == 0), "nt", "ticks", is_graphable, paste0(name, "_ticks_0accesses"), n_breaks, 
         labs(title = "Test Duration vs Number of Threads (no critical section accesses)",
              x     = "Number of Threads",
              y     = "Test Duration"))

gen_graph(subset(ticks_by_test_and_nt, critical_section_accesses == 8), "nt", "ticks", is_graphable, paste0(name, "_ticks_8accesses"), n_breaks, 
         labs(title = "Test Duration vs Number of Threads (1 cache line updated during critical section)",
              x     = "Number of Threads",
              y     = "Test Duration"))

gen_graph(subset(ticks_by_test_and_nt, critical_section_accesses == 80), "nt", "ticks", is_graphable, paste0(name, "_ticks_80accesses"), n_breaks, 
         labs(title = "Test Duration vs Number of Threads (10 cache lines updated during critical section)",
              x     = "Number of Threads",
              y     = "Test Duration"))
