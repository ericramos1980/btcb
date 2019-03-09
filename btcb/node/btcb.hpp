#pragma once

#include <array>

namespace btcb {

namespace bootstrap {

//BETA
static char const BETA_GENESIS_PUBK[] = "C118C71301B585FD8885C5D7E89B6ADABA4FFBEA84833B98983BBC29E14F0AFD";
static char const BETA_GENESIS_ADDR[] = "xrb_3iarrwbi5fe7zp6adjgqx4fpopotbzxyo3659gebigxw79iny4qx8qz1i7zq";
static char const BETA_GENESIS_BLOCK[] = R"%%%({
    "type": "open",
    "source": "C118C71301B585FD8885C5D7E89B6ADABA4FFBEA84833B98983BBC29E14F0AFD",
    "representative": "bcb_3iarrwbi5fe7zp6adjgqx4fpopotbzxyo3659gebigxw79iny4qx8qz1i7zq",
    "account": "bcb_3iarrwbi5fe7zp6adjgqx4fpopotbzxyo3659gebigxw79iny4qx8qz1i7zq",
    "work": "425e04d5179d7472",
    "signature": "C9EA4571C8C8F1C3A4ABBAEE2730E9BED4BA0F37D149137115EE37D6D007655342ED78C87FD10DE1216E469225B2ED022F53543CF85AE91380808AB9CAEBCE0A"
})%%%";

static const std::array<const std::string, 2> BETA_REPRS = {BETA_GENESIS_PUBK,
                                                            "81604FBAFD8792D953B9DA3F1292BCF2E64F5EA577F556658ACC5F5D4A893E92"
                                                           };
static const std::array<const std::string, 8> BETA_PEERS = {
    //bangalore ipv6 and v4; in order
    "2400:6180:100:d0::903:b001",
    "2400:6180:100:d0::903:c001",
    "2400:6180:100:d0::8f7:4001",
    "2400:6180:100:d0::8ff:7001",
    "::ffff:159.65.152.110",
    "::ffff:206.189.142.96",
    "::ffff:139.59.66.31",
    "::ffff:139.59.65.30"
};

//LIVE
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
