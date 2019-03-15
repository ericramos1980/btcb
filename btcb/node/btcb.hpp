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

static const std::array<const std::string, 9> BETA_REPRS = {
    BETA_GENESIS_PUBK,
    "bcb_3djb63zf4d7wexwthhroiahdyqincqymf896yks9kooh4o18iitbnguwmjzo",
    "bcb_11iqjc3h6we5emdbynf9mkxgxwx5skrqc1fcg4hehji6ib93nq6agwuudmm6",
    "bcb_1fqby8riam57frgpquoiur5363ufkgoddym7o74ff7u1zbrmtdgms6eoaxqk",
    "bcb_1peqzid67xyhronqheueoookpyqqg4c61eyxr8sdg8fqkk7iot1oiobf6pgg",
    "bcb_1mscbbq9qhamt1joz1g33wtffgqbdm9ce6g4w1iw9fo3w3cubuue1h4fi663",
    "bcb_1gu3i19ca83y53fatwb9xcnneei85pryi7tdni9emg1strk6s7oyhnasgj95",
    "bcb_1pk4qqykag14chpd89f83ykm55j5dkei8qr7z3j6ur3b6f83tymf4xh45e1y",
    "bcb_1qadj4837q3k7etgrxpgeezcgdr639xh1k3ygpj9fnj991xdtmz8gxsgszka"
};
static const std::array<const std::string, 12> BETA_PEERS = {
    "::ffff:45.55.57.81",
    "2604:a880:800:a1::960:3001",

    "::ffff:68.183.220.27",
    "2a03:b0c0:3:e0::1dc:e001",

    "::ffff:206.189.82.238",
    "2400:6180:0:d1::698:5001",

    "::ffff:138.68.58.210",
    "2604:a880:2:d0::2290:9001",

    "::ffff:139.59.65.30",
    "2400:6180:100:d0::8ff:7001",

    "::ffff:206.189.142.96",
    "2400:6180:100:d0::903:c001"

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
static const std::array<const std::string, 16> LIVE_PEERS = {
    "::ffff:165.227.119.157",
    "2604:a880:800:a1::13bc:9001",

    "::ffff:157.230.100.165",
    "2a03:b0c0:3:e0::1fc:4001",

    "::ffff:188.166.26.165",
    "2a03:b0c0:2:d0::c48:9001",

    "::ffff:159.65.75.19",
    "2604:a880:2:d0::2247:e001",

    "::ffff:139.59.61.182",
    "2400:6180:100:d0::56:c001",

    "::ffff:104.248.156.8",
    "2400:6180:0:d1::447:9001",

    "::ffff:159.89.114.42",
    "2604:a880:cad:d0::bc5:4001",

    "::ffff:178.62.119.213",
    "2a03:b0c0:1:d0::c:6001"
};

}
}
