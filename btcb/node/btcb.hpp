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


static char const LIVE_GENESIS_PUBK[] = "843FB2D436CB6A64B7604D26AC37BCF888AD077DF44351E6EF820A8EA0F4C4D5";
static char const LIVE_GENESIS_ADDR[] = "bcb_333zpdc5fkucekup1mb8oiuusy6aon5qux45c9mgz1icjtihbj8oichgif91";
static char const LIVE_GENESIS_BLOCK[] = R"%%%({
"type": "open",
"source": "843FB2D436CB6A64B7604D26AC37BCF888AD077DF44351E6EF820A8EA0F4C4D5",
"representative": "bcb_333zpdc5fkucekup1mb8oiuusy6aon5qux45c9mgz1icjtihbj8oichgif91",
"account": "bcb_333zpdc5fkucekup1mb8oiuusy6aon5qux45c9mgz1icjtihbj8oichgif91",
"work": "0410caca931183f8",
"signature": "A31E4C1167467F62DDD06C912139241245A2CC3C8A6832B4C232DE76F8CC5EB8EEB3E1F87DD82B3DB4CAC62FDC9B8E06C40D93E7F919301C080C63D101A71C0B"
})%%%";

static const std::array<const std::string, 1> LIVE_REPRS = {LIVE_GENESIS_PUBK};
static const std::array<const std::string, 4> LIVE_PEERS = {"2406:da1a:305:2d01:debd:39b9:d75d:2abd",
                                  "2406:da1c:890:da01:da59:3b9e:ebb1:2489",
                                  "2600:1f16:aaa:ff02:74dc:ff20:bb59:2daa",
                                  "2a05:d01c:e0a:2801:8b79:f9db:ee8f:e673"};

}
}
