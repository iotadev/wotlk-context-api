#include "ContextCache.h"
#include <cassert>
#include <iostream>

int main()
{
    WotlkContext::Cache cache({42}, 250);
    assert(!cache.Due(42, 1000));
    assert(cache.Read().front().State == "unobserved");
    cache.Transition(42, 1000, 5000, true);
    auto generation = cache.Due(42, 1000);
    assert(generation && *generation == 1);
    WotlkContext::Sample sample;
    sample.Guid = 42;
    sample.Generation = *generation;
    sample.SampledMs = 1000;
    sample.WallMs = 5000;
    sample.State = "online";
    sample.Name = "fixture";
    assert(cache.Publish(sample));
    assert(!cache.Due(42, 1249));
    assert(cache.Due(42, 1250));
    auto older = sample;
    older.SampledMs = 999;
    assert(!cache.Publish(older));
    cache.Transition(42, 1100, 5100, false);
    assert(!cache.Publish(sample));
    assert(cache.Read().front().Name.empty());
    assert(cache.Read().front().State == "offline");
    cache.Transition(42, 1200, 5200, true);
    assert(!cache.Publish(sample));
    assert(cache.Due(42, 1200) == 3);
    assert(!cache.Due(99, 1200));
    sample.Generation = 3;
    sample.SampledMs = 1200;
    sample.State = "transferring";
    sample.Name.clear();
    assert(cache.Publish(sample));
    auto copy = cache.Read();
    copy.front().State = "mutated-copy";
    assert(cache.Read().front().State == "transferring");
    std::cout << "WOTLK cache lifecycle checks passed\n";
}
