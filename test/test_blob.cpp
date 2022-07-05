#include <string>

#include "../common/utils/blob.h"

int main() {
    using NATBuster::Common::Utils::Blob;
    using NATBuster::Common::Utils::BlobView;
    using NATBuster::Common::Utils::ConstBlobView;
    using NATBuster::Common::Utils::BlobSliceView;
    using NATBuster::Common::Utils::ConstBlobSliceView;

    Blob hello = Blob::factory_string("Hello");
    Blob test = Blob::factory_string("This is a test");
    Blob world = Blob::factory_string("Cruel_world");
    const Blob punctuation = Blob::factory_string(".?!");

    world.resize(6, 11);
    auto space = test.slice(4, 1);
    auto end = punctuation.cslice(2, 1);

    Blob result = Blob::concat({ &hello, &space, &world, &end}, 0, 1);

    result.resize(result.size() + 1);

    result.getw()[result.size() - 1] = '\0';

    assert(result.size() == 13);

    assert(strcmp("Hello world!", (char*)result.getr()) == 0);

    return 0;
}