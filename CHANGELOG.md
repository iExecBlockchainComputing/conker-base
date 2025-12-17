# Changelog

## [0.2.0](https://github.com/iExecBlockchainComputing/conker-base/compare/v0.1.0...v0.2.0) (2025-12-17)


### Features

* add quote generator assistant ([#45](https://github.com/iExecBlockchainComputing/conker-base/issues/45)) ([e14320b](https://github.com/iExecBlockchainComputing/conker-base/commit/e14320bb3c88d641d1d3e7f95eaf48bbdbfb8584))
* create cvm service, and config ([#21](https://github.com/iExecBlockchainComputing/conker-base/issues/21)) ([2f1843f](https://github.com/iExecBlockchainComputing/conker-base/commit/2f1843fbec0637e2c02a9939fd143d8dd53d47e8))
* create internal package ([#20](https://github.com/iExecBlockchainComputing/conker-base/issues/20)) ([6374d5a](https://github.com/iExecBlockchainComputing/conker-base/commit/6374d5af6de66f47b664ab0f8f099c8f3ede24e2))
* create pkg package and remove unused code ([#19](https://github.com/iExecBlockchainComputing/conker-base/issues/19)) ([cb650f6](https://github.com/iExecBlockchainComputing/conker-base/commit/cb650f64375b5976e4baac10685a810adaaf8a74))
* Improvement of the network cvm assistant ([#28](https://github.com/iExecBlockchainComputing/conker-base/issues/28)) ([0ec9bab](https://github.com/iExecBlockchainComputing/conker-base/commit/0ec9babc23bd672ecdc32d9caa0509f0c4a289b4))
* install fdisk ([#40](https://github.com/iExecBlockchainComputing/conker-base/issues/40)) ([6b61e1e](https://github.com/iExecBlockchainComputing/conker-base/commit/6b61e1ef679c13f405f7243965b182bdca83fdcf))
* key generation from key-provider-agent ([#36](https://github.com/iExecBlockchainComputing/conker-base/issues/36)) ([dd9a85f](https://github.com/iExecBlockchainComputing/conker-base/commit/dd9a85f34c6f5d2326592214042bf9f83c5339b2))
* prevent multiple apps ([#22](https://github.com/iExecBlockchainComputing/conker-base/issues/22)) ([6b41e5c](https://github.com/iExecBlockchainComputing/conker-base/commit/6b41e5c8f9ffc20f44c3a7d44a3c8e02e0c6abe5))
* refactor API and add application orchestrator ([#24](https://github.com/iExecBlockchainComputing/conker-base/issues/24)) ([04aedda](https://github.com/iExecBlockchainComputing/conker-base/commit/04aeddadc494f2f6b9f24b8258b713eee95f2352))
* remove key from log ([#38](https://github.com/iExecBlockchainComputing/conker-base/issues/38)) ([e4c038c](https://github.com/iExecBlockchainComputing/conker-base/commit/e4c038c9eb7e7247a6662701a27080e95c97ac69))
* switch firewall assistant to bash script ([#32](https://github.com/iExecBlockchainComputing/conker-base/issues/32)) ([6300eaf](https://github.com/iExecBlockchainComputing/conker-base/commit/6300eaf7e7c7d71bf05cd62b8800d58bbf8ad520))
* update go version to 1.24 ([#18](https://github.com/iExecBlockchainComputing/conker-base/issues/18)) ([454ac1d](https://github.com/iExecBlockchainComputing/conker-base/commit/454ac1d62ec84bb9d7ee68d1d122fd6d05f1f4d8))


### Bug Fixes

* change default config path ([#25](https://github.com/iExecBlockchainComputing/conker-base/issues/25)) ([ff96755](https://github.com/iExecBlockchainComputing/conker-base/commit/ff9675551b654431f014b3c88f56f0c4672da093))
* clean memory or resource leaks detected during analyses ([#8](https://github.com/iExecBlockchainComputing/conker-base/issues/8)) ([9b46f35](https://github.com/iExecBlockchainComputing/conker-base/commit/9b46f35333167ec2ab8df810e7996ca51cb73f36))
* correct mappername assignement ([#37](https://github.com/iExecBlockchainComputing/conker-base/issues/37)) ([4176add](https://github.com/iExecBlockchainComputing/conker-base/commit/4176add978c5caa9b8e2650f21b03eb78cf1e493))
* correct short and long options for getopt_long ([#7](https://github.com/iExecBlockchainComputing/conker-base/issues/7)) ([a54973e](https://github.com/iExecBlockchainComputing/conker-base/commit/a54973e9cc4b78e80c9432c7ef7c75af752b33b6))
* disable logs redirection and hiding ([#39](https://github.com/iExecBlockchainComputing/conker-base/issues/39)) ([4c82255](https://github.com/iExecBlockchainComputing/conker-base/commit/4c82255c0f4c1612c4f1755309fa740d2e1e5385))
* error on check preventing multiple apps ([#26](https://github.com/iExecBlockchainComputing/conker-base/issues/26)) ([1b853be](https://github.com/iExecBlockchainComputing/conker-base/commit/1b853be4e36992c02674ec1e9712f141522a2832))
* improve quote generator robustness ([#52](https://github.com/iExecBlockchainComputing/conker-base/issues/52)) ([1b583bb](https://github.com/iExecBlockchainComputing/conker-base/commit/1b583bb692fcebff94e125bed204d2e330ec9899))
* issues in several logging statements ([#6](https://github.com/iExecBlockchainComputing/conker-base/issues/6)) ([89dc001](https://github.com/iExecBlockchainComputing/conker-base/commit/89dc0017866399884bf8a53c5ad509bc33710900))
* **key-provider-agent:** improve key provider agent robustness ([#50](https://github.com/iExecBlockchainComputing/conker-base/issues/50)) ([f6eeb7a](https://github.com/iExecBlockchainComputing/conker-base/commit/f6eeb7a373f0ebe74e26184a7eb1eda3b99b803d))
* potential buffer overflows ([#9](https://github.com/iExecBlockchainComputing/conker-base/issues/9)) ([8c5e2ba](https://github.com/iExecBlockchainComputing/conker-base/commit/8c5e2bad262da56156a4335212b9012a9b39f9f6))
* **secret-provider-agent:** improve robustness of provider agent ([#43](https://github.com/iExecBlockchainComputing/conker-base/issues/43)) ([4a9042a](https://github.com/iExecBlockchainComputing/conker-base/commit/4a9042a314befd1e219f324e34aa0c66943c3f99))
