#pragma once

#include <stdio.h>
#include <string.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

template<class T, T Max>
class histogram_log2
{
  static constexpr std::size_t
  floor_log2_const(T x)
  {
    return (x == 0) ? (1/x)
      : (x == 1) ? 0
      : 1 + floor_log2_const(x >> 1);
  }

  static T
  floor_log2(T x)
  {
    return 8 * sizeof(long long) - __builtin_clzll(x) - 1;
  }

  T sum_, min_, max_;
  T zero_;
  T buckets_[floor_log2_const(Max)];
  T over_;
  enum { NBUCKETS = sizeof(buckets_) / sizeof(buckets_[0]) };

  void
  get_print_stats(unsigned *lwidth, unsigned *min_bucket,
                  unsigned *max_bucket) const
  {
    // Compute the label width.  This is fixed, rather than being
    // based on used buckets, so that multiple histograms are
    // comparable.
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (long long unsigned)Max);
    *lwidth = strlen(buf);

    // Find the minimum used bucket
    *min_bucket = 0;
    if (!zero_)
      for (; *min_bucket < NBUCKETS; ++*min_bucket)
        if (buckets_[*min_bucket])
          break;

    // Find maximum used bucket (plus one, so we always end with a
    // zero label)
    if (over_) {
      *max_bucket = NBUCKETS;
    } else {
      *max_bucket = 0;
      for (std::size_t i = 0; i < NBUCKETS; ++i)
        if (buckets_[i])
          *max_bucket = i;
      if (*max_bucket < NBUCKETS)
        ++*max_bucket;
    }
  }

  static const char *
  get_bar(unsigned width)
  {
    static const char bar[] = "================================================================================";
    unsigned barlen = sizeof(bar) - 1;
    assert(width <= barlen);
    return &bar[barlen - width];
  }

public:
  constexpr histogram_log2()
    : sum_(0), min_(~0), max_(0), zero_(0), buckets_{}, over_(0)
  {
  }

  histogram_log2 &
  operator+=(T val)
  {
    sum_ += val;
    if (val < min_)
      min_ = val;
    if (val > max_)
      max_ = val;
    if (val <= 0)
      ++zero_;
    else if (val >= Max)
      ++over_;
    else
      ++buckets_[floor_log2(val)];
    return *this;
  }

  histogram_log2 &
  operator+=(const histogram_log2 &other)
  {
    sum_ += other.sum_;
    if (other.min_ < min_)
      min_ = other.min_;
    if (other.max_ > max_)
      max_ = other.max_;
    zero_ += other.zero_;
    for (std::size_t i = 0; i < NBUCKETS; ++i)
      buckets_[i] += other.buckets_[i];
    over_ += other.over_;
    return *this;
  }

  T
  sum() const
  {
    return sum_;
  }

  T
  min() const
  {
    return min_;
  }

  T
  max() const
  {
    return max_;
  }

  T
  count() const
  {
    T res = zero_ + over_;
    for (T v : buckets_)
      res += v;
    return res;
  }

  T
  mean() const
  {
    return sum_ / count();
  }

  double
  meand() const
  {
    return sum_ / (double)count();
  }

  void
  print_stats() const
  {
    if (count() == 0) {
      printf("count 0\n");
      return;
    }
    printf("count %llu  min %llu  mean %llu  max %llu\n",
           (long long unsigned)count(), (long long unsigned)min(),
           (long long unsigned)mean(), (long long unsigned)max());
  }

  void
  print() const
  {
    unsigned lwidth, min_bucket, max_bucket;
    get_print_stats(&lwidth, &min_bucket, &max_bucket);

    if (zero_)
      printf("%*llu- : %llu\n", lwidth, 0ull, (long long unsigned)zero_);
    for (unsigned i = min_bucket; i <= max_bucket; ++i) {
      bool last = i == max_bucket;
      if (i == NBUCKETS)
        printf("%*llu..: %llu\n", lwidth, (long long unsigned)Max,
               (long long unsigned)over_);
      else
        printf("%*llu%s: %llu\n", lwidth, 1ull<<i,
               last ? "  " : "- ", (long long unsigned)buckets_[i]);
    }
  }

  void
  print_bars() const
  {
    unsigned lwidth, min_bucket, max_bucket;
    get_print_stats(&lwidth, &min_bucket, &max_bucket);

    unsigned max_count = 0;
    for (auto v : buckets_)
      if (v > max_count)
        max_count = v;
    if (max_count == 0) {
      printf("%*llu..: \n", lwidth, 0ull);
      return;
    }

    unsigned bar_width = 75 - lwidth;
    if (zero_)
      printf("%*llu- : %s\n", lwidth, 0ull, get_bar(bar_width * zero_ / max_count));
    for (unsigned i = min_bucket; i <= max_bucket; ++i) {
      bool last = i == max_bucket;
      if (i == NBUCKETS)
        printf("%*llu..: %s\n", lwidth, (long long unsigned)Max,
               get_bar(bar_width * over_ / max_count));
      else
        printf("%*llu%s: %s\n", lwidth, 1ull<<i,
               last ? "  " : "- ",
               get_bar(bar_width * buckets_[i] / max_count));
    }
  }
};
