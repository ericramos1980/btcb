#pragma once

#include <array>

namespace btcb {

namespace bootstrap {

static char const BETA_GENESIS_PUBK[] = "760498A0EE295CBCAB2B755CA290F64AB39362EE52787EE773B8853A2D816570";
static char const BETA_GENESIS_ADDR[] = "bcb_1xi6m4igwccwqkokpxcwncahekomkfjgwnmrhumq9g679apr4sdiiwuoees6";
static char const BETA_GENESIS_BLOCK[] = R"%%%({
"type": "open",
"source": "760498A0EE295CBCAB2B755CA290F64AB39362EE52787EE773B8853A2D816570",
"representative": "bcb_1xi6m4igwccwqkokpxcwncahekomkfjgwnmrhumq9g679apr4sdiiwuoees6",
"account": "bcb_1xi6m4igwccwqkokpxcwncahekomkfjgwnmrhumq9g679apr4sdiiwuoees6",
"work": "f13f55d85e1cb689",
"signature": "DFDCD43AFEFAB6D88A8CBFC00FFB61DA132D53376D49F4C485118266C96D4CD2605BDD4FF97FF450E444ECED37A84827E99C83BDD8CD3A69351B2702699CDE0E"
})%%%";

static const std::array<const std::string, 1> BETA_REPRS = {BETA_GENESIS_PUBK};
static const std::array<const std::string, 4> BETA_PEERS = {"2406:da1a:305:2d01:debd:39b9:d75d:2abd",
                                  "2406:da1c:890:da01:da59:3b9e:ebb1:2489",
                                  "2600:1f16:aaa:ff02:74dc:ff20:bb59:2daa",
                                  "2a05:d01c:e0a:2801:8b79:f9db:ee8f:e673"};

}
}
