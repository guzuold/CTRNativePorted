// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <string>
namespace ctrport {
inline bool pinned_retail_image(const std::string& sha) {
return sha == "1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816" ||
sha == "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5" ||
sha == "42f23dccea56fda157f1169c9c96c28eeb06c9a624b33459624ffee76f108c88" ||
sha == "480a769db6697a8aa3b17019f0efe60095b85f05f8bfc67f13ed3efb675e5c9c" ||
sha == "65d518fa10c13d122346113fa46616564dcab439e42cc43df64e69767bb83d12" ||
sha == "76eeb1d59b60e76ab5ec31c78ba4a532dda1a41af7610e23fbd5bb8299add709" ||
sha == "a70bcf4b76cbb40b4e768d1939480f46f336fd42515e6ebeda5cb164b10b5e77" ||
sha == "ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1" ||
// US CUSA13795: eboot v01.00 and v01.21, libSceFace, libSceHand, libSceS3DConversion
// (libc, libSceFios2 and libSceNpToolkit2 are identical to the EU ones above).
sha == "23ddf3909d87a42a4c74be395865b130e4128b892de59d71082d8620cd8cd858" ||
sha == "e3b7a068b5e7be0f976102aa223a74425e7817c67e26e21416163291750c1b83" ||
sha == "0a212b9dfa6029d580a649874988285d5e315fd03ea8025bb1974e7b71fcc616" ||
sha == "4d4105c1e18808a340fc30b86f8e42b51a814d1585058da70ab3766e59f119ea" ||
sha == "a28cd031c57270117ae45dd015956c41f30941bca06500c5dd67171320efb794";
}
}
