# Matter identity and certification for the TYWE2L replacement module

Decision-support document for the ESP32-H2-MINI-1-H4S open replacement module for the Tuya TYWE2L.

Researched 13 August 2026. Every factual claim below carries a source link. Where a figure is not published anywhere, this document says so rather than guessing. Where sources contradict each other, both are shown.

---

## Bottom line

**"Sourcing certs" is not the answer, because certificates are the cheap part.** Espressif will happily pre-provision your ESP32-H2 modules with a real DAC, PAI and private key. What no vendor sells, at any price, is a **Vendor ID**. A VID comes only from CSA membership, and CSA membership starts at **USD 7,500 per year** plus **USD 3,000 per product** certification plus an unpublished test-lab bill. That is the actual gate, and no amount of certificate shopping gets past it.

For a community retrofit project the recommendation is a three-stage path:

1. **Start on test VID 0xFFF1 with locally generated test DACs.** Zero cost, zero lead time, works today on Apple Home, Alexa, SmartThings and Home Assistant with an "uncertified device" prompt. Blocked on Google Home unless each user registers the device in their own Google Home Developer Console project.
2. **Reserve the production partition layout now** (`esp_secure_cert` 0x2000 and `fctry` 0x6000) even though you flash them with test data. Adding a partition later means changing the partition table, which means a physical reflash. Reserving costs 32 KB of a 4 MB part.
3. **Only if and when the project has a legal entity and revenue,** join CSA as an Adopter, get a VID, and use Espressif's Open PAA to issue production DACs under your own VID. Design for this now; do not pay for it now.

The three findings that most change the picture:

- **Home Assistant tightened in June 2026.** The rebuilt matter.js-based Matter Server "[can no longer add uncertified devices with an official development/test certificate] out of the box". There is an opt-in setting, but the friendliest ecosystem is no longer friendly by default. ([Home Assistant, 23 June 2026](https://www.home-assistant.io/blog/2026/06/23/the-matter-upgrade-youve-been-waiting-for/))
- **CSA explicitly permits using another party's VID with that party's documented consent.** Certification Policy §2.15: "Usage of another party's manufacturer-specific identification is only permitted with a proof of consent of that party." This is a commercial negotiation, not a product anyone advertises, and it is the single most under-explored option for this project. ([Certification Policy 07-4842-14](https://csa-iot.org/wp-content/uploads/2021/12/07-4842-14-ptsc-certification-policy.pdf))
- **A DAC cannot be added by OTA in any documented way.** DACs are unique per device; a Matter OTA image is identical for every device. Espressif has published no field-provisioning flow. Plan the security lifecycle before burning any eFuse.

And one precedent worth reading before writing a line of code: **[automatous-io/shelly-1-gen4-matter-thread](https://github.com/automatous-io/shelly-1-gen4-matter-thread)** is third-party open source Matter-over-Thread firmware for a shipped Shelly product, which is this project's exact shape. Its `CERTIFICATION.md` and `REVERSIBILITY.md` are a ready-made template for how to document identity, trademark, warranty and reversibility. See §5.3a.

---

## Summary table of options

| Option | Money | Lead time | Ecosystems | Effort | Ship legally? |
|---|---|---|---|---|---|
| **A. Test VID 0xFFF1 + self-generated test DAC** | $0 | Immediate | Apple ✓ (prompt), Alexa ✓ (prompt), SmartThings ✓ (prompt), HA ✓ (opt-in setting), chip-tool ✓, **Google ✗** unless user registers | Low. `esp-matter-mfg-tool` one-liner | No. Explicitly not for consumer devices. No Matter logo, no patent licence |
| **B. Test VID + per-user Google Console registration** | $0 | Minutes per user | As above **plus Google ✓** | Low code, high user friction. Each user makes a Google Cloud project | Same as A |
| **C. Espressif VID 0x131B DAC + your own CD** (`dac_origin_vendor_id` remap) | CSA membership + cert still required. DAC price unpublished | Weeks (negotiated) | All, once certified | Medium. Removes the PKI build, not the membership | Yes, once certified |
| **D. CSA Adopter + own VID + Espressif Open PAA DAC** | $7,500/yr + $3,000/product + lab (unpublished, ~$7–10k per a stale 2023 estimate) + DAC cost | Months | All | High | Yes |
| **E. Certification Transfer Program (Associate tier)** | $0/yr + $2,500/product + $500/yr per product | Weeks | All | Low, but **does not fit** a board you designed | Yes, but only for re-badging someone else's unchanged certified product |
| **F. "Borrow" a VID with written consent** | Negotiated. Unpublished | Unknown | All, once certified | Unknown. Needs a willing member | Yes, per Certification Policy §2.15 |

Option A is the start-here answer. Option D is the destination. Option F is the cheap shortcut worth one phone call before committing to D.

---

## 1. The attestation chain

### 1.1 What a DAC actually is

A **Device Attestation Certificate** is a per-device X.509 v3 certificate holding a device-unique ECDSA P-256 key pair. It exists to prove one thing at commissioning: that this physical device is a genuine unit of a product that CSA certified.

Google's primer describes it as "unique per device and associated with the unique attestation key pair within the product. It is issued by a CA associated with the Device manufacturer." ([Google Home Developers](https://developers.home.google.com/matter/primer/attestation))

### 1.2 The three-tier hierarchy

- **PAA (Product Attestation Authority).** Self-signed root. Google: "the Matter trust store is federated and the set of PAA certificates trusted by commissioners is maintained in a central trusted database (the Distributed Compliance Ledger)."
- **PAI (Product Attestation Intermediate).** Signed by a PAA, signs DACs. Carries your Vendor ID, and optionally a Product ID. A vendor may run one PAI per product, per product family, or one for everything.
- **DAC.** Signed by the PAI. Lives on the device alongside the PAI so the device can present the whole chain.

There are two flavours of PAA, and the distinction matters commercially. A **VID-scoped PAA** carries a vendor-id attribute and can only sign PAIs for that one VID. A **Non-VID-scoped PAA**, which CSA calls an "Open PAA", has no vendor-id attribute and can sign PAIs for any VID its operator is asked to serve. CSA lists eighteen approved PAAs, all Non-VID Scoped, described as offering "the service of its well-established Public Key Infrastructure (PKI) provider to other members". ([CSA PAA list](https://csa-iot.org/certification/paa/))

Espressif operates **both**: an "Espressif Matter PAA" scoped to VID `0x131B`, and an "Espressif Matter Open PAA" with no VID attribute. Both are verifiable in the live DCL root-certificate list at `https://on.dcl.csa-iot.org/dcl/pki/root-certificates`. Espressif announced the Open PAA on 1 October 2024. ([Espressif developer blog](https://developer.espressif.com/blog/matter-improvements-to-espressif-dac-provisioning-service/))

### 1.3 The Certification Declaration

The CD is the piece people underestimate. It is **not** an X.509 certificate. It is a CMS SignedData payload (RFC 5652) wrapping a TLV structure, defined in spec section 6.3.1, and it is **signed by CSA, not by you**. You receive it only after your product passes certification.

Its contents: VID, one or more PIDs, device type, Security Level, Security Information, **Certification Type**, and the signature. ([Matter Handbook](https://handbook.buildwithmatter.com/how-it-works/attestation/))

Certification Type takes three values, and the numbering matters when you build test firmware: `0` development, `1` provisional, `2` official. Espressif's certification guide tells you to use "1 (provisional)" while testing and swap to 2 once CSA issues the real one. ([esp-matter certification](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/certification.html))

The CD also carries optional `dac_origin_vendor_id` and `dac_origin_product_id` fields. These exist precisely for the case where the DAC's VID differs from the product's VID, which is what happens when a silicon vendor bakes a DAC into a module under its own VID. Both fields must be present or neither. This is the mechanism behind Option C below.

### 1.4 What the commissioner validates

At commissioning the commissioner generates a random 32-byte attestation nonce, asks the device for its attestation elements, and checks:

- The DAC chain validates DAC → PAI → PAA, "including revocation checks on the PAI and PAA"
- The VID in the DAC matches the VID in the PAI
- The attestation signature is valid over the elements plus the attestation challenge
- "Nonce in Device Attestation Elements matches the nonce provided by the Commissioner"
- "Certificate Declaration Signature is valid using one of the Alliance's well-known Certification Declaration signing keys"
- VID/PID consistency between the CD, the DAC and the Basic Information cluster
- The PAA is present in the DCL trust store

([Google Home Developers](https://developers.home.google.com/matter/primer/attestation), [Silicon Labs](https://docs.silabs.com/matter/latest/matter-device-attestation/))

The nonce defeats signature replay. The DCL lookup is what makes the trust store federated rather than baked into every phone.

### 1.5 The DCL

The **Distributed Compliance Ledger** is a Cosmos SDK / CometBFT permissioned blockchain, public to read, Proof-of-Authority to write, validated by member companies. ([CSA](https://csa-iot.org/certification/distributed-compliance-ledger/), [source repo](https://github.com/zigbee-alliance/distributed-compliance-ledger))

Write roles, from the repo's `docs/transactions.md`:

| Role | What it can do |
|---|---|
| Trustee | Create and approve accounts, approve root certificates |
| Vendor | "can add models that belong to the vendor ID associated with the vendor account" |
| VendorAdmin | Add and edit vendor info and any model |
| CertificationCenter | "can certify and revoke models" |
| NodeAdmin | Add validator nodes |

Two consequences worth internalising. A Vendor account is **bound to one VID**, so shipping under someone else's VID means their account makes the DCL writes, visibly. And `CERTIFY_MODEL` requires CertificationCenter, meaning **you cannot self-certify**.

Is a DCL entry needed for a device to work? For certification, yes, mandatory. For commissioning, not strictly, except on Google Home. A device whose PAA is absent from the DCL simply fails attestation and produces the uncertified-device dialog that Apple, Alexa, SmartThings and Home Assistant let the user dismiss.

One secondary effect for this project: Home Assistant uses the DCL for OTA metadata. No DCL entry means no firmware updates through that channel.

**No DCL write fee is published anywhere.** The repo documents no economic model, which is consistent with a permissioned chain where access is gated by membership rather than payment. Treat "writes are free once you have an account" as a reasonable inference, not a sourced fact.

---

## 2. Test credentials

### 2.1 The test VID range

`0xFFF1`, `0xFFF2`, `0xFFF3` and `0xFFF4` are reserved test Vendor IDs. Google states the rule plainly: **"A test VID cannot be used in a consumer device."** ([Google, Get started](https://developers.home.google.com/matter/get-started))

And more specifically on scope: "VIDs `0xFFF1` — `0xFFF4` are reserved for testing. They may be used for basic commissioning and control tests, but they can't be used during the following phases of development: Test, Field Trial, OTA." ([Google, Troubleshooting](https://developers.home.google.com/matter/troubleshooting))

Amazon says the same for its prototype flow: "Prototype products must use the default Vendor ID, `FFF1`", and the credentials "are specifically designed to test prototype devices. Don't use these credentials to secure production devices." ([Amazon ACK docs](https://developer.amazon.com/en-US/docs/alexa/ack/matter-provision-device.html))

*Uncertainty flag:* the verbatim normative spec text on VID allocation could not be retrieved, because the Matter specification sits behind a CSA download request. The rule above is quoted from ecosystem documentation, which is consistent across Google, Amazon and Silicon Labs, but it is not the spec itself.

### 2.2 The CHIP test PAA and test credentials

The Matter SDK ships a complete test PKI in `credentials/test/` of [project-chip/connectedhomeip](https://github.com/project-chip/connectedhomeip):

- `credentials/test/attestation/` holds pre-generated DACs and PAIs. Test VID `FFF1` with PIDs `8000` through `8000-0007`, plus VID `FFF2` PID `8001`, in both DER and PEM. There are deliberately broken variants too ("Wrong-Prefix", CRL and CDP variants) for negative testing.
- The default test root is `Chip-Test-PAA-NoVID-Cert.pem` / `Chip-Test-PAA-NoVID-Key.pem`, a non-VID-scoped test PAA.
- `credentials/test/certification-declaration/` holds test CDs such as `Chip-Test-CD-FFF2-8001.der`.

Generating a working test identity for the ESP32-H2 is a single command:

```
esp-matter-mfg-tool -v 0xFFF2 -p 0x8001 --vendor-name "test vendor" \
    --product-name "test product" --hw-ver 1 --hw-ver-str "hardware version" --pai \
    -k $MATTER_SDK_PATH/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem \
    -c $MATTER_SDK_PATH/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem \
    -cd $MATTER_SDK_PATH/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der
```

([esp-matter-tools mfg_tool README](https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool))

For certification testing proper, Espressif's guide tells you **not** to use the SDK's default PAA but to generate your own with `chip-cert gen-att-cert --type a --subject-vid 0x131B ...`. And the Matter Handbook is explicit that "Test DACs are not permitted during certification testing".

### 2.3 What each ecosystem actually does today

| Ecosystem | Test VID accepted? | Registration needed? | Tied to a user account? | What the user sees |
|---|---|---|---|---|
| **Apple Home** | Yes | No | No | "Uncertified Accessory" dialog, tap **Add Anyway** |
| **Google Home** | **Only if pre-registered** | **Yes**, a Matter integration in the Google Home Developer Console | **Yes**, the commissioning user must be a project member or Field Trial user | Nothing if registered. A **hard block** if not |
| **Amazon Alexa** | Yes | No | No | "This device isn't Matter compatible", dismissible |
| **SmartThings** | Yes | No | No | Uncertified prompt, tap **Continue** / **Add Anyway** |
| **Home Assistant (Matter Server 9.x, HA 2026.7+)** | **No by default** | n/a | No | Refuses unless the opt-in setting is enabled |
| **chip-tool / chip-repl** | Yes | No | No | Nothing. `--paa-trust-store-path` points at the test PAAs |

Sources: [Tuya interoperability docs](https://developer.tuya.com/en/docs/iot-device-dev/interoperability?id=Kd307fzbmfsij), [Google pairing docs](https://developers.home.google.com/matter/integration/pair), [Amazon ACK](https://developer.amazon.com/en-US/docs/alexa/ack/matter-provision-device.html), [Silicon Labs Amazon setup](https://docs.silabs.com/matter/2.8.1/matter-ecosystems/amazon-ecosystem-setup), [matter.js ECOSYSTEMS.md](https://github.com/matter-js/matter.js/blob/main/docs/ECOSYSTEMS.md), [Home Assistant blog](https://www.home-assistant.io/blog/2026/06/23/the-matter-upgrade-youve-been-waiting-for/).

**Google is a pure allowlist, not a warning.** Tuya's documentation draws the distinction better than anyone:

> "If you get a prompt for an uncertified product when adding a product to the Home app, Tuya-enabled app, or SmartThings app, tap **Add Anyway** to continue pairing."
>
> "When adding an uncertified Matter device to the Google Home app, you will receive a prompt that **prevents you from adding the device**."

For a shared-registry community project this is the single worst constraint. Every end user would need their own Google Cloud project with a Matter integration registered against the same VID/PID, and would need to be a project member or enrolled Field Trial user. That is not a viable distribution model for non-technical users.

There is also a hard ceiling on test PIDs. Google's codelab: "the PIDs `0x8000` to `0x801F` have test (Device Attestation Certificates) DACs that can be used for examples. Unless you're planning on setting up your own DAC, use a PID within this range." That is 32 usable PIDs per test VID, or 128 across all four. Google's Matter Virtual Device tool is tighter still: "Only test vendor ID `0xFFF1` is supported" and "Only test product IDs `0x8000` through `0x801F` are supported." ([codelab](https://developers.home.google.com/codelabs/matter-device), [Virtual Device](https://developers.home.google.com/matter/tools/virtual-device))

*Uncertainty flag:* no numeric cap on the **number of test devices** per Google project or account appears in any public Google document, and none was found. If such a limit exists it lives in the console UI. The 32-PID pool is the documented ceiling.

**SmartThings developer mode is not an attestation bypass.** This is a common misconception worth killing. Developer mode ("Long-press the About SmartThings menu option for ten seconds") exists to surface your own unpublished products from the Certification Console in the device catalogue. You do not need it to onboard a test-cert device; the ordinary Add Anyway prompt handles that. ([SmartThings docs](https://developer.smartthings.com/docs/devices/enable-developer-mode))

Note also that SmartThings retired its Matter documentation section; `/docs/devices/matter*` URLs now redirect to a 404 and Matter content sits under "Hub Connected Devices". Old links are dead.

### 2.4 What changed in the last 18 to 24 months

- **June 2026, Home Assistant tightened.** The Open Home Foundation Matter Server was rebuilt on matter.js and shipped in HA 2026.7. Uncertified devices with development/test certificates "can no longer be added out of the box", and certificate revocation is now checked at commissioning. There **is** an opt-in: "Developers who work with test-net OTA firmware and uncertified Matter devices should enable the corresponding option in the Matter Server configuration page." The matterjs-server CLI exposes `--enable-test-net-dcl` (env `ENABLE_TEST_NET_DCL=true`). ([HA blog](https://www.home-assistant.io/blog/2026/06/23/the-matter-upgrade-youve-been-waiting-for/), [matterjs-server](https://github.com/matter-js/matterjs-server))
- **python-matter-server is end-of-life.** Version 8.1.2 is "the final version of the python-matter-server, libraries, and docker-container". Anything you write against the old Python server is on a dead branch. ([repo](https://github.com/home-assistant-libs/python-matter-server))
- **September 2025, Google loosened slightly.** A release note added OTA "through the DCL for devices not registered in Google Home Developer Console".
- **2026, CSA Interop Test Lab transition** at Google. Affects certification workflow only.
- **Nothing changed upstream.** CSA policy on test VIDs and the spec text are unchanged from Matter 1.0 through 1.6. The only genuine policy reversal in the window is Home Assistant's.

The direction of travel is towards stricter attestation, not looser. Architect on that assumption.

---

## 3. Buying identity

### 3.1 Espressif pre-provisioned modules

**What you get.** Modules arrive with the per-device manufacturing partition already flashed at module manufacture. From the ESP32-H2 production docs:

> "ESP32-H2 modules can be pre-flashed with the manufacturing partition images during module manufacturing itself and then be shipped to you. This saves you the overhead of securely generating, encrypting and then programming the partition into the device at your end. Please contact your Espressif contact person for more information."

You also get a manifest CSV and a bundle of QR code images.

**The CD is not included, and cannot be.** Same page:

> "In Espressif Matter Prep-provisioning modules, the DAC key pair, DAC and PAI certificates are pre-flashed by default."

DAC key pair, DAC, PAI. No Certification Declaration, because CSA only issues your CD after your product passes certification. You generate the `fctry` partition carrying the CD yourself afterwards with `esp-matter-mfg-tool -cd ...`. ([production docs](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/production.html))

**ESP32-H2 support is first-class, not an afterthought.** The production page above is the esp32h2 build of the docs and names ESP32-H2 modules explicitly. The December 2022 launch announcement covers "all Espressif SoCs supporting the Matter protocol, including ESP32, ESP32-C3, ESP32-C2, ESP32-H2 and ESP32-S3". ([Espressif news](https://www.espressif.com/en/news/Pre-provisioning_Matter))

Your specific part is confirmed as the 4 MB variant: the datasheet ordering table lists **ESP32-H2-MINI-1-H2S** at 2 MB and **ESP32-H2-MINI-1-H4S** at 4 MB. ([datasheet](https://documentation.espressif.com/esp32-h2-mini-1_mini-1u_datasheet_en.html))

**MOQ, price and lead time: no public figures exist.** Every Espressif page routes to sales. The Matter solution page says "Please contact us to get more details about pricing and deployment options." Contacts are `sales@espressif.com` and, for PKI specifically, `matter-pki@espressif.com`. Any MOQ number you have heard is unsourced.

**Whose VID?** Espressif can do either. Their FAQ documents the third-party-DAC route explicitly:

> "Cooperate with your platform vendor (Espressif): The platform vendor may embed a DAC in a chip or platform module using their VID/PID. CD is used to remap VID/PID using the dac_origin_vid/dac_origin_pid fields."

([esp-faq](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-matter.html))

That gets you a DAC under Espressif's VID `0x131B`. It does **not** get you a CD, and the CD is still issued to you against your own VID. So this removes the PKI problem, not the membership problem. Espressif's own certification doc is blunt: "You need to become a member of CSA and request a Vendor ID code from CSA Certification before you apply for a Matter Certification."

The same applies to ESP-ZeroCode modules: "Since these are Matter modules, you need to be a CSA Member and get your product certified by CSA. Espressif can assist you in this process. However, you can order test modules for your product right away." ([ZeroCode docs](https://docs.zerocode.espressif.com/docs/usage-scenarios/create-product/))

**The esp_secure_cert partition.** Custom partition type `0x3F`, default offset `0xD000`, 8 KiB in the modern TLV format:

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, 0x3F, , 0xD000, 0x2000, encrypted
```

> "Please note that, TLV format uses compact data representation and hence partition size is kept as 8KiB."

Two legacy formats exist at `0x6000` (24 KiB): `cust_flash` raw, and `nvs`. Contents are the device certificate, the CA certificate and the private key; with the DS peripheral the key is stored as ciphertext encrypted by the DS peripheral. ([format docs](https://github.com/espressif/esp_secure_cert_mgr/blob/main/docs/format.md))

⚠️ **Documented contradiction to resolve before committing a layout.** The format doc and the esp-matter light example both mark this partition `encrypted`. The ESP-FAQ shows the line with a comment saying "Never mark this as an encrypted partition", in the context of the pre-provisioning service. That is live drift in Espressif's own documentation. Ask Espressif which applies to pre-provisioned modules specifically, since a partition written at their factory has different constraints from one you flash yourself.

**esp-matter-mfg-tool.** Install with `python3 -m pip install esp-matter-mfg-tool`. Per device it emits into `out/<vid_pid>/<uuid>/`: the NVS factory partition binary, an onboarding-codes CSV (QR string, manual pairing code, passcode, discriminator), a QR PNG, and an `internal/` directory with the DAC cert, DAC key and PAI cert. A `staging/` directory carries the shared PAI plus a DAC list and summary CSV.

Useful flags: `-n 5` for five devices, `--dac-in-secure-cert --target esp32h2` to move the DAC and key out of NVS into `esp_secure_cert`, and `-ds --efuse-key-id 1 --port /dev/ttyUSB0` to program the eFuse-backed key. The tool only generates images; flashing is a separate `esptool.py write_flash`.

### 3.2 Other DAC and PKI providers

CSA lists eighteen approved PAAs, all Non-VID Scoped: Aurora Networks, Cybertrust Japan, DigiCert, Dream Security, **Espressif Systems**, Kudelski IoT (Americas), Kudelski IoT (EMEA/APAC), Lumi United, Nexus, NXP, SEALSQ, SEALSQ Taiwan, Snowball Technology, SSL.com, STMicroelectronics, StrongKey, TrustAsia, Tuya Global. ([CSA PAA list](https://csa-iot.org/certification/paa/))

Notably **absent**: GlobalSign, Keyfactor, Entrust, Infineon, Microchip, Silicon Labs, Nordic. Infineon, Microchip and Silicon Labs all resell **Kudelski**.

| Provider | Own VID available? | Public pricing | Notes |
|---|---|---|---|
| **Espressif** | Yes (Open PAA), or theirs (0x131B) | None | Best-documented path for your silicon. `matter-pki@espressif.com` |
| **Kudelski keySTREAM** | Customer VID required | **None.** AWS Marketplace: "request a private offer" | The white-label engine behind Silicon Labs, Infineon and Microchip. Free test DACs for evaluation. Requires signing a "Requestor Agreement Document" before any PAI is created |
| **DigiCert Device Trust Manager** | Customer VID required | None | Portal, REST, EST, SCEP delivery |
| **SSL.com** | Customer VID, branded PAI | None | The **only** vendor publishing a lead time: PAI issuance "typically completes within 1–3 business days"; DAC issuance "near-instantaneous per device" |
| **SEALSQ / WISeKey** | Customer VID | None | VaultIC292 secure element "can be pre-configured" with DAC and PAI. 2023-vintage material |
| **Infineon OPTIGA Trust M MTR** | Customer VID via Kudelski | Eval shield $39.14 at Digi-Key | ⚠️ Marketed as pre-provisioned, but Infineon's own repo says "It is expected from the Customer to perform 'late-stage provisioning'". **The part does not arrive with your DAC in it** |
| **NXP EdgeLock 2GO** | Customer VID | None | Free trial mentioned in search snippets; both NXP product pages 404'd, so this is weakly sourced |
| **StrongKey** | Customer VID | None | Open-source-leaning. Possibly the most receptive cold call for an unusual arrangement, though that is inference |
| **Keyfactor EJBCA** | You run your own PAA | Software | Requires an HSM and a CSA policy audit. Not realistic below serious volume |
| **GlobalSign** | — | — | Rule out. Not a PAA, no Matter product, page 404s |

**Not one provider sells production DACs under its own VID as an off-the-shelf product.** Every one of them requires you to already hold a VID. That is the finding that answers the original question.

For context on lead times, Silicon Labs publishes the only hard end-to-end figure anyone does, for their own CPMS pipeline: "Sample generation typically takes from 4 to 6 weeks to produce, assuming all of the integrations with Kudelski IoT are in place to receive DACs." ([Silicon Labs CPMS](https://docs.silabs.com/matter/latest/matter-using-cpms/))

*Uncertainty flags:* Microchip could not be researched (microchip.com returned 403 throughout). The part number `ATECC608B-TFLXWSSC` could not be confirmed to exist on Microchip, Digi-Key or Mouser; documented TrustFLEX suffixes are TFLXTLS and TFLXWPC, and "WSSC" looks more like Wi-SUN than Matter. Verify before designing around it.

### 3.3 Borrowing a VID

This is the most interesting and least advertised option, and the policy text is permissive:

> "A company SHALL only use their own manufacturer-specific identification to identify their own devices and their functionality. **Usage of another party's manufacturer-specific identification is only permitted with a proof of consent of that party.** This usage policy also applies to the Certification Transfer Program."

([Certification Policy 07-4842-14 §2.15](https://csa-iot.org/wp-content/uploads/2021/12/07-4842-14-ptsc-certification-policy.pdf))

Read carefully: using another party's VID is **not** categorically banned. It is banned **without documented consent**. That is a licensing arrangement between two parties, not a membership bypass. Nobody advertises it because it is negotiated rather than productised, but the policy language anticipates exactly the arrangement a small project would want from a silicon vendor or contract manufacturer.

⚠️ Caveat: 07-4842-14 is dated 14 April 2022 and its programme sections still describe Zigbee 3.0 and Green Power. No newer public revision could be found; probes for revisions 15, 16 and 17 at the same URL pattern all 404'd. Matter-specific rules may sit in a members-only document. Verify the current revision with CSA before relying on this.

**Contract manufacturer VIDs.** The Associate tier existing at $0/yr specifically to enable white-labelling tells you the arrangement is normal and priced for. But no vendor publicly advertises VID lending. Tuya, for instance, is a **PAA**, not a VID lender: they "received Non-VID Scoped PAA certification authorization", which "allows Tuya to sign PAI for any Vendor Identification (VID) obtained from the Alliance", and "developers need only to provide the VID applied from the Alliance". Their certification service advertises "2 weeks at the earliest" with no pricing.

**No lab, consultancy or reseller was found offering to sponsor a small maker under its own VID.** UL, Granite River Labs and the IoT consultancies all publish no prices and route back to "you join CSA".

---

## 4. CSA membership and certification

### 4.1 Tiers and fees

Live figures from [csa-iot.org/become-member](https://csa-iot.org/become-member/), effective **1 April 2026**:

| Tier | Annual fee | Certify own product | Own derivative | Certification Transfer |
|---|---|---|---|---|
| **Associate** | **$0** | not permitted | not permitted | $2,500 + $500/yr per product |
| **Adopter** | **$7,500** | **$3,000** | $2,500 | $2,500 |
| **Participant** | **$21,500** | **$2,000** | $1,500 | $1,500 |
| **Promoter** | **$112,500** + unpublished one-time initiation | $2,000 | $1,500 | $1,500 |

The page footnotes read: "* Promoter membership also requires a one-time initiation fee" and "* Membership Fees effective April 1, 2026."

This was a fee rise. The superseded figures still sit in the page's HTML comments: Adopter $7,000, Participant $20,000, Promoter $105,000. Roughly a 7% increase across the board, apparently the first in some years. Per-product certification fees appear unchanged.

**There are no free certifications bundled with any tier.** Every certification is a per-product fee on top of dues. Certifications are valid "for the lifetime of the product" with no annual renewal, except the Associate CTP path which carries $500/yr per product.

⚠️ **Two published inconsistencies.** CSA's own [Adopter one-pager PDF](https://csa-iot.org/wp-content/uploads/2025/02/adopter_membership_one_pager_v6.pdf) states the correct $7,500 annual fee but then quotes "$2,000 USD per product and ... $1,500 USD for derivative products", which are the **Participant** numbers. The live table ($3,000 / $2,500) is independently corroborated by the Certification Transfer Program page. Treat the one-pager's per-product line as an error. The Participant one-pager still quotes the pre-April-2026 $20,000.

### 4.2 Can an Adopter get a VID and ship?

**Yes, unambiguously.** CSA's own tier description: "Adopter Members use existing, approved specifications to build products. Adopters can access completed, approved standards documents, certify products through Alliance certification programs, and use Alliance technology logos and trademarks for certified products."

Silicon Labs states it directly: "The minimum membership level required for Matter Certification when developing your own product is the Adopter level. Once you are a member, you will need to request a Vendor ID." ([Silicon Labs](https://docs.silabs.com/matter/latest/matter-certification/))

**Adopter is the entry point for a company shipping its own certified Matter product.** Participant is not required.

What Adopter does **not** get, and this is the real trade-off:

1. No working group participation or voting. Adopter Member Agreement §4: "Unless otherwise determined by the Board of Directors, Adopter shall not be entitled to any voting rights with respect to the business or proceedings of the Alliance."
2. No draft or early spec access.
3. Higher per-product fees ($3,000 vs $2,000). Break-even against the $14,000 dues gap is roughly 14 products a year, so the fee maths almost never justifies upgrading on its own.
4. **Cannot be a source in the Certification Transfer Program.** An Adopter can receive a transfer, not supply one.
5. ⚠️ **Cannot use FastTrack Recertification self-testing.** This is the strongest argument for Participant and it is easy to miss. FastTrack lets "Alliance Participant and Promoter Member companies that are trained in the proper use of Matter Certification tools ... perform their own testing of updated products and obtain Matter certification of the result for no additional Alliance fee", such that "recertification fees paid to ATLs and the Alliance can be reduced to zero". Scope is limited to security fixes, critical bug fixes, interoperability fixes and spec-version updates. ([FastTrack FAQ](https://handbook.buildwithmatter.com/certification/fast-track-recertification-faq/))

   For a product with a long support life and regular firmware updates, an Adopter pays ATL fees plus $3,000 each time; a Participant can drive that to zero. On a five-year product that likely dominates the $14,000 annual difference. There is a partial workaround: an Adopter or Associate holding a certification obtained via CTP can get an "Expedited Transfer" with no certification fee when their transfer partner FastTracks the original.

### 4.3 Getting a Vendor ID

> "A Vendor ID is used to identify the manufacturer of products and is required before certifying a product. Vendor IDs are assigned by the Alliance and appear in the device firmware and in the attestation certificates ... Companies should send requests for new Vendor IDs to help@csa-iot.org or through the Contact Us page."

([Matter Handbook](https://handbook.buildwithmatter.com/certification/certifying-a-product/membership/))

Membership is a hard prerequisite. Certification Policy §2.8.1: "To submit a Product or Platform for certification or compliance testing and to be granted certification, a company must be a member in good standing of the Connectivity Standards Alliance."

**No separate VID fee is published anywhere.** Policy §2.15 says only "Upon request, the Connectivity Standards Alliance assigns to a Member a manufacturer-specific identification". Everything indicates it comes with membership at no extra charge, but no CSA sentence saying "free" could be found. **No SLA on turnaround time is published either.**

**Can an individual get one?** No published route, and the paperwork points the other way. The Adopter Member Agreement's signature page requires "Legal Corporate Name", "Corporate Email", "Corporate Phone" and "Corporate Website". The Certification Policy consistently says "a company must be a member". The agreement contemplates a "consortium, association or other similar organization" as an Adopter, but never a natural person. **Expect to need a legal entity.**

⚠️ Silicon Labs claims Associate level suffices for a VID ("An associate-level membership or higher is required to obtain membership perks, certification, and a Vendor ID"). That is a **single-source vendor claim** and it contradicts CSA's own tier table, which says Associate is "Limited to rebrand/white-label certified products only" and requires partnering with a Participant or Promoter through the CTP. **This is load-bearing for the cheapest possible option. Confirm directly with `help@csa-iot.org` before planning around it.**

### 4.4 Certification cost

**CSA fee:** $3,000 per product at Adopter, $2,000 at Participant or Promoter.

**Test-lab fee:** quote-only, universally. Certification Policy §2.14: "Testing fees are set by the individual authorized test laboratories. Certification fees are set by Connectivity Standards Alliance and vary based on the type of membership."

CSA lists 33 authorised test providers, including Allion, Bureau Veritas, DEKRA, Element, Eurofins, Granite River Labs, Novus Labs, Resillion, TTA, Teledyne LeCroy, Trident IoT, TÜV Rheinland, TÜV SÜD and UL Solutions. ([CSA testing providers](https://csa-iot.org/certification/testing-providers/)) Note that **7layers and SGS are not on the current list**.

⚠️ The only published cost estimate found is from a **March 2023** Tuya article putting lab testing at "$7,000 to $10,000" per product and the application fee at "$2,000 and $3,000". The application half matches CSA's figures, which lends some credibility, but it is three and a half years old. **Get quotes; do not budget on this.**

**Costs the Matter fee does not cover.** Transport certification is separate and comes from other organisations: Thread Group, Bluetooth SIG (for BLE commissioning), and Wi-Fi Alliance if applicable. A March 2023 article quoted Bluetooth at "$9,600 per product for Adopter status" and Thread at "$1,000-$1,500 per product"; **these are unverified and dated.** For a Thread-only ESP32-H2 module you need Thread and Bluetooth, not Wi-Fi.

**Timeline:** CSA publishes none. "The total duration of the certification process is dependent on several factors, including the length of time required by the chosen test provider." Queue depth at the lab is usually the binding constraint. Ask two or three ATLs for current lead times.

**SVE attendance is not required** to certify an ordinary product. The Specification Validation Event's formal role is validating the test plan itself. Where it does matter is qualifying for FastTrack: "there are two paths to completion. The first path is to attend a specification validation event (SVE); the second path is to attend Rapid Recertification Training offered during Alliance Member meetings."

### 4.5 Derivative and similarity paths

**Certification by Similarity (CbS).** Policy §7: "allows a CSA Product that is derived from a previously tested and certified CSA Product to be granted certification based on its similarity ... It is not intended to eliminate the requirement that a Product actually passes CSA compliance tests."

Key rules: only CSA can grant it, no test lab can; it addresses "changes such as color, enclosures, language, etc."; **no chaining** (you cannot base a CbS on a product that itself got CbS); and a **three-year clock** (if the parent is older than three years, full testing is required). Process is a Testing Exemption Request Form to `certification@csa-iot.org`, with an assessment "within one (1) calendar week".

What still triggers full retest, per §7.7: "Addition/exposure of a new feature and/or cluster to the CSA firmware"; "HW, SW or FW changes for the device(s) that the Stack and app are running on"; "Layout change of the module used". And notably "Code refactoring, fixing a security bug, CCB fix; new stack drop" also lands in **Full Testing** under this 2022 policy, which is precisely the pain FastTrack was created to fix.

**Product Family Certification.** §9: certify multiple direct variants of a single Parent Product on one application, where "The Parent Product SHALL be the most feature complete variant". Examples: regional plug variants, depopulated low-cost variants.

**Portfolio Certification**, announced 6 January 2025, lets manufacturers "certify multiple Portfolio Member Products within a single application, using a Parent Product as the basis", and CSA suggests it may supersede CbS and Product Family over time. ⚠️ Detailed rules and fees are **not published publicly**. If you plan a product range, ask CSA for the program document.

**Matter Compliant Platform Certification** launched **30 September 2025**. For the first time in Matter, silicon vendors can certify platforms, hardware and SDKs. First cohort: **Espressif, Nordic, NXP and Silicon Labs**. ([CSA newsroom](https://csa-iot.org/newsroom/matter-compliant-platform-certification-building-on-proven-foundations-for-faster-trusted-smart-home-innovation/))

⚠️ **Read the claim precisely.** CSA says "Device makers that adopt Matter Compliant Platforms benefit from faster development cycles, lower certification costs, and simplified recertification", and that platform or SDK updates "may not require full re-testing". It does **not** say an end product skips certification. **You still certify your product, you still need your own VID, you still pay the per-product fee.** What it reduces is test scope. Espressif's certification application guidance confirms the mechanism: the CSA application form has a "Compliant Platform" field where you "select ESP32-C-Series or ESP32-H-Series from the dropdown list".

**Certification Transfer Program (CTP).** The one route where you genuinely ship under someone else's certificate. Enrollers must be Participant or Promoter; recipients can be Adopter or Associate. Permitted changes are "Only branding or appearance changes to the product packaging, enclosure, and/or user interface (e.g., acceptable changes include: enclosure color, brand markings, and UI splash screen)". Policy §8.1 is stricter still: "No hardware changes other than a new Product enclosure and no software changes."

**This does not fit a board you designed yourself.** CTP is for re-badging someone else's finished product unchanged. It only applies to this project if you abandon your own PCB and re-badge a ZeroCode-class module. And per §2.15 the recipient still needs their own VID.

### 4.6 Logo and trademark

Governing document: [Brand Name, Trademark, and Logo Usage Guidelines, December 2024](https://csa-iot.org/wp-content/uploads/2024/12/TM_Logo-Use-Guide_Update_December-2024.pdf).

Two-part test to use the branding: **member in good standing** AND **that specific product certified**, with the certification held by the company making the claim.

> "Any use of Alliance Brands by any entity other than Alliance members in good standing is strictly prohibited, except as may be permitted by law or these Guidelines."

Prohibited uses that catch people out:

- Identifying products that are not certified, or implying certification when there is none
- **No "Matter" in a domain or subdomain.** The guidelines' own examples of prohibited forms: `www.newco.matter.com`, `www.matter.newco.com`, `www.learnaboutmatter.com/newco`
- **No "Matter" in a social handle.** "@NewCoMatter is prohibited"
- **Not part of your product or brand name**
- No coined terms: "Matter-ize", "Zigbee-ify", "Matterhorn", "Zig-switch" are all named as prohibited

**What you may say while uncertified**, if you are a member: forward-looking statements only, which "cannot suggest or imply Product is compliant, certified, or currently supports the Alliance Tech". CSA's approved example: "Our plans for the HomeRun app include adding Matter for in-home device controls." Non-members get no forward-looking allowance at all.

For this project, practically: **you may say the module speaks the Matter protocol; you may not use the Matter word mark or logo, and you must not imply certification.** Choose a project name and domain that do not contain "Matter".

### 4.7 The patent licence, and why it matters here

CSA's IPR Policy grants a **RANDz Licence**, and the scope is the sting:

> "'RANDz License' means a no cost, worldwide, perpetual, irrevocable ... non-exclusive, non-transferable license to the Necessary Claims of Adopted Specifications ... solely to make, have made, use, import, sell, offer to sell, license, promote and/or otherwise distribute and dispose of the resulting product or technology **that is Certified** with the applicable Adopted Specifications"

([CSA IPR Policy 6.3](https://csa-iot.org/wp-content/uploads/2022/09/CSA-IPR-Policy-6.3-Adopted.pdf))

The licence is zero cost, but it is scoped **solely to Certified products**. **Shipping an uncertified Matter implementation gets you no patent cover.** It is also reciprocal: joining commits you to license back any patents you hold that read on Matter.

The Matter SDK being Apache 2.0 grants you copyright and the contributors' patent rights in their contributions. It does **not** grant you the CSA membership pool's patent licence, and it grants you nothing on trademark. These are three separate things and they are routinely conflated in community discussion.

---

## 5. The awkward question: a retrofit module in someone else's product

### 5.1 Who is the manufacturer?

For certification purposes, **whoever holds the VID and submits the application**. CSA's model has no concept of a retrofit or an aftermarket module inside a third party's enclosure. The Certification Transfer Program is the closest analogue and it runs the opposite direction: it covers re-badging a certified product, not re-hearting an uncertified one.

Concretely, if this project ever certifies, the certified article would be **your module**, under **your VID**, as a component or as a device-typed product in its own right. The Tuya device it goes into is irrelevant to CSA. It is very relevant to other regulators (see 5.4).

### 5.2 Can a community project legally distribute firmware carrying a VID/PID?

Distinguish three claims that get muddled:

1. **Implementing the spec.** The SDK is Apache 2.0. Writing and distributing Matter firmware is not itself prohibited by CSA, and no evidence was found of CSA ever objecting publicly to an open source project, issuing a takedown, or making a policy statement against test-VID use in the wild.
2. **Patent exposure.** The RANDz licence covers **Certified** products only. An uncertified Matter device has no patent cover from the CSA pool. In practice nobody has enforced against hobbyist projects, but the legal position is genuinely unprotected rather than merely grey.
3. **Trademark.** Firmly prohibited. You cannot call it Matter-branded, use the logo, or imply certification.

Shipping firmware that carries **test** VID 0xFFF1 is what every SDK example and every open project does. Shipping firmware that carries **someone else's real** VID without written consent breaches Certification Policy §2.15 and is straightforwardly not on. Shipping firmware with a VID you invented is worse: it collides with a real vendor's assignment.

### 5.3 What comparable projects actually do

| Project | Identity used | Notes |
|---|---|---|
| **Tasmota** (Berry Matter implementation) | **VID 0xFFF1, PID 0x8000** | Verified in source, [`Matter_zz_Device.be`](https://github.com/arendst/Tasmota/blob/development/lib/libesp32/berry_matter/src/embedded/Matter_zz_Device.be): `static var VENDOR_ID = 0xFFF1`, `static var PRODUCT_ID = 0x8000`. The [Tasmota docs](https://tasmota.github.io/docs/Matter/) say it without hedging: "Tasmota cannot be Matter certified, it uses development vendor id's, which typically raise user warnings when commissioning the device." Google Home "only works after following these instructions" (the Console registration) |
| **matter.js** (and everything built on it) | **VID 0xFFF1, PID 0x8000** | "Please use 0xFFF1 as Vendor ID and 0x8000 as product id because matter.js uses this by the current example scripts by default." Its README is candid: "To release a Matter product (device, bridge, or controller), you must be a member of the Connectivity Standards Alliance (CSA) and obtain certification ... matter.js based projects show up as 'uncertified test devices' in the ecosystems." ([ECOSYSTEMS.md](https://github.com/matter-js/matter.js/blob/main/docs/ECOSYSTEMS.md)) |
| **Matterbridge** | **VID 0xFFF1, vendor name "Matterbridge"** | Verified in `packages/core/src/matterbridge.ts`: `VendorId(getIntParameter('vendorId') ?? 0xfff1)`, `getParameter('vendorName') ?? 'Matterbridge'`, PID `?? 0x8000` |
| **home-assistant-matter-hub** | 0xFFF1, vendor name `t0bst4r` | Verified in `packages/backend/src/core/app/options.ts` |
| **ioBroker.matter** | 0xFFF1, vendor name `ioBroker` | Verified in `src/main.ts` and `BridgedDevicesNode.ts` |
| **Home Assistant / Open Home Foundation** | **Real VID 4939 (0x134B)**, controller side only | See below. The one genuine exception |
| **Silicon Labs / Nordic / Espressif sample apps** | Test VIDs | SDK-wide defaults in `CHIPDeviceConfig.h` are VID `0xFFF1`, PID `0x8001`, vendor name literally `"TEST_VENDOR"`. Espressif's Kconfig: `DEVICE_VENDOR_ID` default `0xFFF1`, commented "Defaults to test VID 0xFFF1". Nordic's certified-platform build switches to their real 0x127F |
| **ESPHome** | **No Matter support at all** | ⚠️ **Correcting a common assumption.** ESPHome added **OpenThread** in 2025.6, not Matter. The [components index](https://esphome.io/components/) has no Matter entry. The [OpenThread docs](https://esphome.io/components/openthread/) are explicit: "Thread by itself does not allow controlling devices: It is just a communication protocol. To control the Thread devices, a higher-level protocol is required: Matter or Apple HomeKit or ESPHome API." Feature requests [#1430](https://github.com/esphome/feature-requests/issues/1430) (open since 2021) and [#2722](https://github.com/esphome/feature-requests/issues/2722) remain unimplemented. ESPHome has no VID because it has nothing to put one on |

The reserved test range is defined in SDK source, [`CHIPVendorIdentifiers.hpp`](https://github.com/project-chip/connectedhomeip/blob/master/src/lib/core/CHIPVendorIdentifiers.hpp): `TestVendor1 = 0xFFF1u` through `TestVendor4 = 0xFFF4u`, with an `IsTestVendorId()` helper. Development DACs exist for FFF1/FFF2/FFF3 across PIDs 0x8000–0x801F in `credentials/development/attestation/`; note 0xFFF4 is reserved but ships no dev DACs.

**The one open source organisation with a real VID.** The **Open Home Foundation holds VID 4939 (0x134B)**, confirmed directly from the CSA's ledger at `https://on.dcl.csa-iot.org/dcl/vendorinfo/vendors/4939`:

```json
{"vendorID":4939, "vendorName":"Home Assistant (Open Home Foundation)",
 "companyLegalName":"Open Home Foundation",
 "vendorLandingPageURL":"https://www.openhomefoundation.org/"}
```

It is wired into the software. The Matter Server add-on launches with `--vendorid 4939`, and `python-matter-server`'s `vendor_info.py` carried the telling comment `# add nabucasa vendor while we're not yet certified`. Certification landed in March 2025 as two separate certificates, deliberately split so the UI does not need recertifying on every release: "Certification for the Open Home Foundation Matter Server means it properly connects and communicates with other Matter devices, while certification for Home Assistant is about being able to display the Matter trademark." ([HA blog](https://www.home-assistant.io/blog/2025/03/10/matter-certification/))

**But VID 4939 has zero device models registered in the DCL** (`/dcl/model/models/4939` returns not found). Their certifications are for a controller and a software component. They have never shipped a Matter *device* under it. And they were candid that funding made it possible: "Certification would be very difficult for any other open-source project, but we have the funding first to build a great server and also to pay for the required testing."

**Scale check.** A direct query of the production DCL returns **459 registered vendors and 4,948 certified device models across 423 VIDs, and no test VID appears anywhere in it.** That is the entire commercial Matter device universe. Small vendors that did get real VIDs include Shelly 0x1490 (22 models), SONOFF/ITEAD 0x1321 (31), Espressif 0x131B (13), Arduino 0x1515 (1), and Athom B.V. / Homey 0x143C at the Adopter tier. openHAB, Raspberry Pi and Matterbridge are all absent.

**So: every open-source Matter device or bridge ships on test VID 0xFFF1, customising only the vendor *name* string. There is no known case of an open source device project obtaining a real VID and certifying.** That absence is itself the answer.

The clearest statement of why comes from matter.js maintainer Ingo Fischer in [Matterbridge discussion #12](https://github.com/Luligu/matterbridge/discussions/12):

> "without being a CSA member and certifying the device (matterbridge in this case) and then getting real per-device-instance certificates (and each step costs money) matterbridge will stay a 'test device' without proper certificates. And this generates this message."
>
> "And yes controllers need to tell the user that he pairs an (uncertificated and pot unsecure) device and allow him to 'Opt out'."

### 5.3a The direct precedent: third-party firmware inside a shipped product

**[automatous-io/shelly-1-gen4-matter-thread](https://github.com/automatous-io/shelly-1-gen4-matter-thread)** (231 stars) is "The first third-party open source Matter over Thread firmware for Shelly Gen4 devices". This is precisely this project's scenario: third-party code going into someone else's shipped, regulatory-marked product. It has thought the problem through, and its [CERTIFICATION.md](https://github.com/automatous-io/shelly-1-gen4-matter-thread/blob/main/docs/CERTIFICATION.md) is the best maker-side statement found anywhere:

> "Matter and Thread certification are designed for commercial products manufactured and distributed at scale. The process requires paid membership in the certifying organization, per-product certification fees, and testing through an authorized lab. Together these run into thousands of dollars per year plus a per-device cost. That cost structure is built for companies shipping volume products, not for open source firmware that individuals flash onto hardware they already own."
>
> "Test credentials are appropriate for personal use, development, and open source distribution. They are not appropriate for a commercial product, which would require a real VID/PID from the CSA and certification."

It presents as manufacturer "AUTOMATOUS.IO" on the esp-matter test VID, and documents the exact ecosystem prompts, a trademark disclaimer naming CSA, Thread Group and Allterco, warranty voiding, factory-key destruction, and a burned eFuse marker recording that non-vendor firmware ran.

**Copy this pattern.** It is a working template for exactly this project's documentation: CERTIFICATION.md and REVERSIBILITY.md alongside the code.

It is also a data point against the "CSA is hostile to makers" thesis: **Jonathan Hui, the Thread/OpenThread lead at Google, publicly highlighted the project on LinkedIn in May 2026**, per the project's own "In the wild" section.

Its one gap is instructive: it addresses warranty, support and reversibility, but says nothing about CE/FCC/RCM re-certification when the radio's operating mode changes. Do better on that front (see 5.4).

### 5.4 Regulatory: replacing the radio module voids the host's approvals

This deserves a flag even though it sits outside Matter.

The original Tuya device carries CE, FCC and RCM marks that were granted for a specific configuration including a specific radio module. Swapping the TYWE2L (2.4 GHz Wi-Fi/BLE, ESP8285) for an ESP32-H2 (802.15.4 and BLE, different radio, different antenna, different emissions profile) is a change to the radio. The host product's existing approvals do not carry over.

For a user modifying their own device this is generally their business. For **anyone distributing modified devices or modules commercially**, in Australia this engages the ACMA supplier obligations and RCM labelling, and equivalent obligations exist under the EU Radio Equipment Directive and FCC Part 15. A pre-approved module can reduce but does not eliminate the burden, and the ESP32-H2-MINI-1 does carry its own modular approvals.

**Recommendation:** frame the project as supplying firmware and bare modules to people who modify their own equipment, and say explicitly in the documentation that fitting the module voids the host device's regulatory approvals and any remaining warranty. Do not sell modified devices. This is a project-shape decision, not a legal opinion, and it is worth a conversation with someone who does this for a living before any commercial step.

---

## 6. Practical recommendation

### 6.1 Ranked

**1. START HERE: test VID 0xFFF1 with self-generated test DACs.**

Cost $0, lead time zero. Works today on Apple Home, Alexa, SmartThings, chip-tool, and Home Assistant with the opt-in setting enabled. Users tap "Add Anyway" once. Document that clearly in the flashing tool and the registry so it is not a surprise.

Handle the two real constraints honestly:
- **Google Home users are blocked.** Document it up front. Offer the Developer Console registration procedure as an advanced path for users who want it, but do not pretend it scales.
- **Home Assistant users on 2026.7+ must enable the uncertified-device option** in the Matter Server configuration page. Put this in the getting-started docs, because the default now refuses.

Use `0xFFF1` with a PID from `0x8000`–`0x801F`. Do **not** invent a PID outside that range unless you are minting your own DACs, because Google's test DAC pool only covers that window.

Give each device a **unique discriminator and passcode**. The flashing tool must generate these per unit, not ship a constant. `esp-matter-mfg-tool` does this natively; the web flasher needs to generate the `fctry` image client-side per device rather than serve one blob. This costs nothing now and is essential later.

**2. Ask Espressif about the Open PAA and about a consented VID arrangement.**

One email to `matter-pki@espressif.com` and `sales@espressif.com`. Two questions worth asking together:
- What does Open PAA DAC issuance cost, at what MOQ, once we hold a VID?
- Would Espressif consent to the project shipping DACs under VID 0x131B with `dac_origin_vendor_id` remap, per your own FAQ and CSA Certification Policy §2.15?

The second question is unusual and the answer is probably no, but the cost of asking is one email and the policy explicitly contemplates it.

**3. CSA Adopter membership, own VID, Espressif Open PAA DACs.**

Only once there is a legal entity and a funding model. Budget the published fees ($7,500/yr + $3,000/product) and treat the lab bill as the largest unknown; get quotes from three ATLs before committing. Confirm with CSA whether Associate ($0/yr) can genuinely obtain a VID, since Silicon Labs claims it can and CSA's own table implies it cannot.

If the project expects to ship regular firmware updates over a multi-year support life, model Participant ($21,500/yr) against Adopter, because FastTrack Recertification is Participant-and-above and takes recertification cost to zero.

**4. Certification Transfer Program.** Listed for completeness. Does not fit a board you designed.

### 6.2 Migration path, and what to build now

The good news is that the architecture for the cheap start and the expensive destination is **identical**. The only thing that changes is the contents of two partitions.

**Do now, costs nothing:**

- **Reserve `esp_secure_cert` (0x2000 at 0xD000) and `fctry` (0x6000) in the partition table**, even flashed with test data. 32 KB out of 4 MB. Adding a partition later means changing the partition table, which on a secure-boot device is a serial operation and a truck roll.
- **Build the firmware against the provider abstraction, not hardcoded certs.** Set `CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y` and read attestation data from `fctry` / `esp_secure_cert` rather than compiling it in. Then swapping test identity for production identity is a data change, not a code change.
- **Watch this gotcha:** `CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL` defaults to `"nvs"`, not `"fctry"`, and the esp-matter light example never overrides it. Out of the box the `fctry` partition sits unused. Set the label explicitly.
- **Generate per-device unique discriminator, passcode and serial from day one.**
- **Keep VID/PID in the device profile schema in the GitHub registry**, so a profile can later carry a real VID without a firmware redesign.

**Do NOT do now:**

- **Do not burn eFuses.** Not the DS/HMAC key, not flash-encryption release mode, not secure boot. Every eFuse bit is one-way: "Each eFuse is a one-bit field which can be programmed to 1 after which it cannot be reverted back to 0." Release mode additionally sets Secure Download Mode on first boot, which "permanently limits the available commands". Once there, the device is field-updatable only through your own signed firmware path. For a community project where users reflash their own hardware, this is exactly the wrong posture.
- **Do not enable flash encryption in release mode** on user-flashed devices.

### 6.3 Can a DAC be added post-hoc in the field?

**The flash content: yes, mechanically. The eFuses: no. And the real blocker is neither.**

- **`fctry`** is an ordinary NVS partition. `nvs_flash_init_partition("fctry")` plus normal NVS writes works from a running app.
- **`esp_secure_cert`** now has a documented write API: `esp_secure_cert_erase_partition()`, `esp_secure_cert_append_tlv()`, `esp_secure_cert_append_tlv_batch()`, with `ESP_SECURE_CERT_WRITE_MODE_FLASH` described as "Runtime provisioning on device". The README lists "Runtime provisioning of credentials" as an explicit use case. The erase is irreversible by the API's own words. ⚠️ **Doc drift:** `docs/format.md` in the same repo still says modification is unsupported. The README is newer; verify against the component version you pin.
- **Flash encryption does not block field writes.** "OTA updates to encrypted partitions will automatically write encrypted data if the function `esp_partition_write` is used."

**But here is the catch that dominates all of the above: a Matter OTA image is one binary delivered to every device, and DACs are unique per device.** That is exactly why Espressif moved them out of the app image: "Because the DACs are unique to every device, the manufacturing partition will also be unique per device."

**You cannot ship a DAC in an OTA image.** What you *can* do is OTA firmware that then fetches its own credential from a server over a vendor-specific channel and writes it locally. **No Espressif-documented "provision DAC over OTA" pattern exists.** That is the most confident negative finding in this research. esp-matter's only direct statement is a warning: "These options are not recommended for devices that are already in field."

**Practical read for this project:** because end users flash their own devices with your desktop/web tool, you have a serial path to every device anyway. The migration from test identity to production identity is a **reflash through the same tool**, not an OTA. That is a genuinely comfortable position and it is a strong argument for keeping eFuses unburned. Design the flashing tool so that re-running it on an already-flashed device updates `fctry` and `esp_secure_cert` in place without wiping user data or Thread credentials.

---

## 7. Flash partitioning implications, 4 MB ESP32-H2 with OTA

### 7.1 The stock table is already the 4 MB table

Verbatim from [`esp-matter/examples/light/partitions.csv`](https://github.com/espressif/esp-matter/blob/main/examples/light/partitions.csv):

```
# Name,   Type, SubType, Offset,  Size, Flags
esp_secure_cert,  0x3F, ,0xd000,    0x2000, encrypted
nvs,      data, nvs,     0x10000,   0xC000,
nvs_keys, data, nvs_keys,,          0x1000, encrypted
otadata,  data, ota,     ,          0x2000
phy_init, data, phy,     ,          0x1000,
ota_0,    app,  ota_0,   0x20000,   0x1E0000,
ota_1,    app,  ota_1,   0x200000,  0x1E0000,
fctry,    data, nvs,     0x3E0000,  0x6000
```

Resolved (4,194,304 bytes total):

| Partition | Offset | Size | Notes |
|---|---|---|---|
| bootloader | 0x0 | 48 KB | |
| partition table | 0xC000 | 4 KB | `CONFIG_PARTITION_TABLE_OFFSET=0xC000`, not the IDF default 0x8000 |
| `esp_secure_cert` | 0xD000 | 0x2000 (8 KB) | type 0x3F, `encrypted` |
| (gap) | 0xF000 | 4 KB | |
| `nvs` | 0x10000 | 0xC000 (48 KB) | |
| `nvs_keys` | 0x1C000 | 0x1000 | `encrypted` |
| `otadata` | 0x1D000 | 0x2000 | |
| `phy_init` | 0x1F000 | 0x1000 | |
| `ota_0` | 0x20000 | 0x1E0000 (1.875 MiB) | |
| `ota_1` | 0x200000 | 0x1E0000 (1.875 MiB) | |
| `fctry` | 0x3E0000 | 0x6000 (24 KB) | NVS subtype |
| `coredump` | 0x3F0000 | 0x10000 | only in `all_device_types_app` |

The inline comment claiming "initial 36K (9 sectors) are reserved" is **stale**; the real reservation runs to 0xD000, which is 52 KB.

Useful variants:
- [`partitions_wifi_thread.csv`](https://github.com/espressif/esp-matter/blob/main/examples/light/partitions_wifi_thread.csv) squeezes both slots to `0x1EA000` (1.914 MiB), the largest dual-slot 4 MB layout Espressif ships.
- [`partitions.h2.csv`](https://github.com/espressif/esp-matter/blob/main/examples/all_device_types_app/partitions.h2.csv), commented "ESP32-H2: Single app (non-OTA)", gives one 3.79 MiB slot.

### 7.2 Does it fit?

Espressif publishes measured sizes for the H2 light example ([optimizations](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/optimizations.html)):

| Config | D/IRAM | Flash |
|---|---|---|
| Default `light` (H2) | 179,487 | **1,576,436** |
| `CONFIG_ENABLE_CHIP_SHELL=n` | 178,695 | 1,521,816 (−54,620) |
| `CONFIG_NEWLIB_NANO_FORMAT=y` | 179,487 | 1,529,228 (−47,208) |
| BLE optimisations | 177,753 | 1,552,372 (−24,064) |

**1,576,436 into 1,966,080 leaves 389,644 bytes, about 20% headroom.** Shell off plus nano-format takes you to roughly 1.47 MB, about 25% headroom. Workable but not comfortable, and the trend is the wrong way: each Matter revision adds clusters, and the light example's `sdkconfig.defaults` already disables well over 100 clusters explicitly to save space.

**1.875 MB is enough for a light, switch, relay or sensor today. 1.7 MB would be tight.** Note the H2 build is roughly 100 KB *larger* than the equivalent C3 Wi-Fi build.

⚠️ **RAM is at least as tight as flash.** The H2 has 320 KB SRAM total; the default light build uses 179,487 bytes of D/IRAM and leaves about 44 KB free heap at boot. Most of Espressif's optimisation table exists to claw heap back rather than flash.

### 7.3 Does attestation data actually compete for space?

**Barely.** `esp_secure_cert` (8 KB) plus `fctry` (24 KB) is **32 KB**, 0.76% of the part. Set against 1.875 MB app slots with 390 KB spare, attestation storage is not the constraint. The constraints are the two app slots and NVS.

The real decision is **dual-slot OTA versus single-slot**:

- Dual slot costs you 1.875 MB and gives A/B rollback plus Matter OTA. Matter certification requires OTA support "either by using Matter-based OTA or vendor specific means".
- Single slot gives 3.79 MB of app but makes every update a serial reflash. Espressif ships this layout for the H2, so it is sanctioned, but it forecloses Matter OTA.
- **Delta OTA does not help.** [`esp_delta_ota`](https://components.espressif.com/components/espressif/esp_delta_ota) shrinks the *download*, not the *storage*: the patch is applied on the fly into the passive app partition, so you still need two full-size slots.

**Recommendation: keep dual-slot.** Given users flash via your tool, you could argue for single-slot, but OTA is a certification requirement on the path to Option D and 20% headroom is adequate for the device types in scope.

### 7.4 NVS sizing

Espressif's only concrete published figure, from the [ESP32-H2 certification guide](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/certification.html) §4.6.3:

> "the minimum NVS size required is 48 KB (0xC000) when using a single endpoint with a group cluster."

More endpoints with the group cluster need more. `MAX_FABRICS` in the [connectedhomeip ESP32 Kconfig](https://github.com/project-chip/connectedhomeip/blob/master/config/esp32/components/chip/Kconfig) is `range 5 255, default 5`, and the light example sets `CONFIG_LWIP_IPV6_NUM_ADDRESSES=6` commented "MAX_FABRIC + 1", so 5 fabrics is the assumed working point.

⚠️ **No Espressif document gives bytes-per-fabric or bytes-per-ACL-entry.** [esp-matter issue #1267](https://github.com/espressif/esp-matter/issues/1267) shows a user hitting the wall and closed without a published answer. Treat 0xC000 as the floor and validate empirically if you expect near 5 fabrics with meaningful ACLs. Note the Thread-specific variant `partitions_thread.csv` drops nvs to 0x6000, contradicting the 48 KB minimum; treat that as a size-constrained test config, not guidance.

### 7.5 ESP32-H2 security silicon

Confirmed from [`soc_caps.h`](https://github.com/espressif/esp-idf/blob/master/components/soc/esp32h2/include/soc/soc_caps.h):

```
SOC_HMAC_SUPPORTED 1        SOC_DIG_SIGN_SUPPORTED 1
SOC_ECDSA_SUPPORTED 1       SOC_ECC_SUPPORTED 1
SOC_FLASH_ENC_SUPPORTED 1   SOC_SECURE_BOOT_SUPPORTED 1
SOC_SECURE_BOOT_V2_RSA 1    SOC_SECURE_BOOT_V2_ECC 1
SOC_EFUSE_SECURE_BOOT_KEY_DIGESTS 3
SOC_FLASH_ENCRYPTION_XTS_AES_128 1
SOC_EFUSE_ECDSA_KEY 1
```

So: **DS peripheral yes, HMAC yes, hardware ECDSA yes, flash encryption yes (XTS-AES-128), Secure Boot v2 yes** with 3 key digests and revocation.

For Matter specifically, the ECDSA route is the one that matters, since DAC keys are ECDSA P-256 and the DS peripheral is RSA-oriented. esp-matter's security doc for H2 recommends it: "ESP32-H2 supports ECDSA hardware peripheral with the ECDSA key programmed in the eFuse. This key is software read protected (in default mode). This peripheral can help to protect the identity of the DAC private key on the device."

⚠️ **Two doc inconsistencies.** `esp-matter-mfg-tool`'s README says "Currently, only esp32h2 supports DS peripheral", while `configure_esp_secure_cert.py`'s README says it configures the DS peripheral on "ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C5", omitting H2. The `soc_caps.h` header settles that the silicon has it. The READMEs are loose with terminology.

---

## 8. Open questions to resolve before committing money

1. **Can a CSA Associate ($0/yr) actually obtain a VID?** Silicon Labs says yes; CSA's own tier table implies no. Email `help@csa-iot.org`. This is the single highest-value unknown.
2. **Espressif pre-provisioning MOQ, price and lead time.** Nothing is published. `sales@espressif.com` / `matter-pki@espressif.com`.
3. **Would Espressif consent to a VID arrangement** under Certification Policy §2.15?
4. **Current revision of the CSA Certification Policy.** The public one is from April 2022 and reads as a Zigbee document. Ask for the current Matter revision.
5. **Portfolio Certification rules and fees.** Not published.
6. **Three ATL quotes** for a Matter certification campaign on an ESP32-H2 Thread device, plus current queue lead times.
7. **Thread Group and Bluetooth SIG fees**, current figures. The ones circulating are from 2023 and unverified.
8. **`esp_secure_cert` encrypted-or-not** for pre-provisioned modules. Espressif's own docs contradict each other.
9. **CSA IPR policy scope for non-members.** The RANDz licence text is quoted in §4.7, but whether the necessary-claims licence runs only between members was not fully established. This is the most important open legal question for a project shipping uncertified.
10. **Regulatory obligations for a retrofit RF module** under EU RED "substantial modification", FCC KDB 996369 modular approval, and ACMA RCM supplier obligations. Not researched here. For a module that changes an existing product's radio from Wi-Fi/BLE to 802.15.4/BLE, **this is probably a larger exposure than the Matter VID question** and deserves its own investigation.
11. **CSA enforcement history.** No evidence was found of any CSA takedown, cease and desist, or policy statement aimed at an open source project using test VIDs. Treat that as "not found in a bounded search", not as "CSA permits it".

---

## Appendix: primary sources

**CSA**
- [Become a member (fees)](https://csa-iot.org/become-member/)
- [Certification Policy 07-4842-14, April 2022 (PDF)](https://csa-iot.org/wp-content/uploads/2021/12/07-4842-14-ptsc-certification-policy.pdf)
- [IPR Policy 6.3 (PDF)](https://csa-iot.org/wp-content/uploads/2022/09/CSA-IPR-Policy-6.3-Adopted.pdf)
- [Trademark and Logo Usage Guidelines, Dec 2024 (PDF)](https://csa-iot.org/wp-content/uploads/2024/12/TM_Logo-Use-Guide_Update_December-2024.pdf)
- [Certification Transfer Program](https://csa-iot.org/certification/transfer-program/)
- [Approved PAA list](https://csa-iot.org/certification/paa/)
- [Authorised testing providers](https://csa-iot.org/certification/testing-providers/)
- [Matter Compliant Platform Certification, 30 Sep 2025](https://csa-iot.org/newsroom/matter-compliant-platform-certification-building-on-proven-foundations-for-faster-trusted-smart-home-innovation/)
- [Interop Lab and two new certification programs, 6 Jan 2025](https://csa-iot.org/newsroom/driving-innovation-with-the-alliance-interop-lab-and-two-new-certification-programs/)
- [Distributed Compliance Ledger](https://csa-iot.org/certification/distributed-compliance-ledger/) and [source](https://github.com/zigbee-alliance/distributed-compliance-ledger)

**Attestation**
- [Google Home: Attestation primer](https://developers.home.google.com/matter/primer/attestation)
- [Matter Handbook: Attestation](https://handbook.buildwithmatter.com/how-it-works/attestation/)
- [Silicon Labs: Matter Device Attestation](https://docs.silabs.com/matter/latest/matter-device-attestation/)
- [connectedhomeip test credentials](https://github.com/project-chip/connectedhomeip/tree/master/credentials/test/attestation)

**Ecosystems**
- [Google: Get started](https://developers.home.google.com/matter/get-started) / [Troubleshooting](https://developers.home.google.com/matter/troubleshooting) / [Pair](https://developers.home.google.com/matter/integration/pair) / [Codelab](https://developers.home.google.com/codelabs/matter-device)
- [Amazon ACK: Provision Matter prototype devices](https://developer.amazon.com/en-US/docs/alexa/ack/matter-provision-device.html)
- [SmartThings: Enable developer mode](https://developer.smartthings.com/docs/devices/enable-developer-mode)
- [Tuya: Interoperability](https://developer.tuya.com/en/docs/iot-device-dev/interoperability?id=Kd307fzbmfsij)
- [matter.js ECOSYSTEMS.md](https://github.com/matter-js/matter.js/blob/main/docs/ECOSYSTEMS.md)
- [Home Assistant: The Matter upgrade you've been waiting for, 23 Jun 2026](https://www.home-assistant.io/blog/2026/06/23/the-matter-upgrade-youve-been-waiting-for/)
- [Home Assistant: Home Assistant officially Matters, 10 Mar 2025](https://www.home-assistant.io/blog/2025/03/10/matter-certification/)
- [matterjs-server](https://github.com/matter-js/matterjs-server) / [python-matter-server (EOL)](https://github.com/home-assistant-libs/python-matter-server)

**Espressif**
- [Production considerations, ESP32-H2](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/production.html)
- [Matter certification, ESP32-H2](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/certification.html)
- [Security, ESP32-H2](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/security.html)
- [Optimizations, ESP32-H2](https://docs.espressif.com/projects/esp-matter/en/latest/esp32h2/optimizations.html)
- [esp-matter-tools / mfg_tool](https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool)
- [esp_secure_cert_mgr format](https://github.com/espressif/esp_secure_cert_mgr/blob/main/docs/format.md)
- [Open PAA announcement, 1 Oct 2024](https://developer.espressif.com/blog/matter-improvements-to-espressif-dac-provisioning-service/)
- [Pre-provisioning launch, Dec 2022](https://www.espressif.com/en/news/Pre-provisioning_Matter)
- [ESP32-H2-MINI-1 datasheet](https://documentation.espressif.com/esp32-h2-mini-1_mini-1u_datasheet_en.html)
- [esp-faq: Matter](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-matter.html)

**Open project practice**
- [automatous-io/shelly-1-gen4-matter-thread](https://github.com/automatous-io/shelly-1-gen4-matter-thread) and its [CERTIFICATION.md](https://github.com/automatous-io/shelly-1-gen4-matter-thread/blob/main/docs/CERTIFICATION.md). The closest precedent to this project
- [Tasmota Matter docs](https://tasmota.github.io/docs/Matter/) and [`Matter_zz_Device.be`](https://github.com/arendst/Tasmota/blob/development/lib/libesp32/berry_matter/src/embedded/Matter_zz_Device.be)
- [matter.js](https://github.com/matter-js/matter.js) / [Matterbridge discussion #12](https://github.com/Luligu/matterbridge/discussions/12)
- [ESPHome OpenThread component](https://esphome.io/components/openthread/) and [components index](https://esphome.io/components/)
- [connectedhomeip `CHIPVendorIdentifiers.hpp`](https://github.com/project-chip/connectedhomeip/blob/master/src/lib/core/CHIPVendorIdentifiers.hpp)
- DCL live queries: `https://on.dcl.csa-iot.org/dcl/vendorinfo/vendors/4939` (Open Home Foundation), `/vendors/4891` (Espressif), `/dcl/pki/root-certificates`

**Other**
- [Matter Handbook: Membership](https://handbook.buildwithmatter.com/certification/certifying-a-product/membership/)
- [Matter Handbook: FastTrack Recertification FAQ](https://handbook.buildwithmatter.com/certification/fast-track-recertification-faq/)
- [Silicon Labs: Matter Certification](https://docs.silabs.com/matter/latest/matter-certification/) / [CPMS](https://docs.silabs.com/matter/latest/matter-using-cpms/)
- [Tasmota Matter vendor ID discussion](https://github.com/arendst/Tasmota/discussions/22582)
