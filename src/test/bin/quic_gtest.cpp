/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

--*/

#include "quic_gtest.h"
#ifdef QUIC_CLOG
#include "quic_gtest.cpp.clog.h"
#endif

bool TestingKernelMode = false;
bool PrivateTestLibrary = false;
const MsQuicApi* MsQuic;
QUIC_CREDENTIAL_CONFIG ServerSelfSignedCredConfig;
QUIC_CREDENTIAL_CONFIG ServerSelfSignedCredConfigClientAuth;
QUIC_CREDENTIAL_CONFIG ClientCertCredConfig;
QuicDriverClient DriverClient;

class QuicTestEnvironment : public ::testing::Environment {
    QuicDriverService DriverService;
    const QUIC_CREDENTIAL_CONFIG* SelfSignedCertParams;
    const QUIC_CREDENTIAL_CONFIG* ClientCertParams;
public:
    void SetUp() override {
        CxPlatSystemLoad();
        ASSERT_TRUE(QUIC_SUCCEEDED(CxPlatInitialize()));
        ASSERT_TRUE((SelfSignedCertParams =
            CxPlatGetSelfSignedCert(
                TestingKernelMode ?
                    CXPLAT_SELF_SIGN_CERT_MACHINE :
                    CXPLAT_SELF_SIGN_CERT_USER,
                FALSE
                )) != nullptr);

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        ASSERT_TRUE((ClientCertParams =
            CxPlatGetSelfSignedCert(
                TestingKernelMode ?
                    CXPLAT_SELF_SIGN_CERT_MACHINE :
                    CXPLAT_SELF_SIGN_CERT_USER,
                TRUE
                )) != nullptr);
#endif
        if (TestingKernelMode) {
            printf("Initializing for Kernel Mode tests\n");
            const char* DriverName;
            const char* DependentDriverNames;
            QUIC_RUN_CERTIFICATE_PARAMS CertParams;
            CxPlatZeroMemory(&CertParams, sizeof(CertParams));
            CxPlatCopyMemory(
                &CertParams.ServerCertHash.ShaHash,
                (QUIC_CERTIFICATE_HASH*)(SelfSignedCertParams + 1),
                sizeof(QUIC_CERTIFICATE_HASH));
            CxPlatCopyMemory(
                &CertParams.ClientCertHash.ShaHash,
                (QUIC_CERTIFICATE_HASH*)(ClientCertParams + 1),
                sizeof(QUIC_CERTIFICATE_HASH));
            if (PrivateTestLibrary) {
                DriverName = QUIC_DRIVER_NAME_PRIVATE;
                DependentDriverNames = "msquicpriv\0";
            } else {
                DriverName = QUIC_DRIVER_NAME;
                DependentDriverNames = "msquic\0";
            }
            ASSERT_TRUE(DriverService.Initialize(DriverName, DependentDriverNames));
            ASSERT_TRUE(DriverService.Start());
            ASSERT_TRUE(DriverClient.Initialize(&CertParams, DriverName));
        } else {
            printf("Initializing for User Mode tests\n");
            MsQuic = new(std::nothrow) MsQuicApi();
            ASSERT_TRUE(QUIC_SUCCEEDED(MsQuic->GetInitStatus()));
            memcpy(&ServerSelfSignedCredConfig, SelfSignedCertParams, sizeof(QUIC_CREDENTIAL_CONFIG));
            memcpy(&ServerSelfSignedCredConfigClientAuth, SelfSignedCertParams, sizeof(QUIC_CREDENTIAL_CONFIG));
            ServerSelfSignedCredConfigClientAuth.Flags |=
                QUIC_CREDENTIAL_FLAG_REQUIRE_CLIENT_AUTHENTICATION |
                QUIC_CREDENTIAL_FLAG_DEFER_CERTIFICATE_VALIDATION |
                QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED;
#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
            memcpy(&ClientCertCredConfig, ClientCertParams, sizeof(QUIC_CREDENTIAL_CONFIG));
            ClientCertCredConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
#endif
            QuicTestInitialize();
        }
    }
    void TearDown() override {
        if (TestingKernelMode) {
            DriverClient.Uninitialize();
            DriverService.Uninitialize();
        } else {
            QuicTestUninitialize();
            delete MsQuic;
        }
        CxPlatFreeSelfSignedCert(SelfSignedCertParams);
#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        CxPlatFreeSelfSignedCert(ClientCertParams);
#endif
        CxPlatUninitialize();
        CxPlatSystemUnload();
    }
};

//
// This function is called by the platform independent test code when it
// encounters kind of failure. Note - It may be called on any thread.
//
void
LogTestFailure(
    _In_z_ const char* File,
    _In_z_ const char* Function,
    int Line,
    _Printf_format_string_ const char* Format,
    ...
    )
{
    UNREFERENCED_PARAMETER(Function);
    char Buffer[128];
    va_list Args;
    va_start(Args, Format);
    (void)_vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, Format, Args);
    va_end(Args);
    QuicTraceLogError(
        TestLogFailure,
        "[test] FAILURE - %s:%d - %s",
        File,
        Line,
        Buffer);
    GTEST_MESSAGE_AT_(File, Line, Buffer, ::testing::TestPartResult::kFatalFailure);
}

struct TestLogger {
    const char* TestName;
    TestLogger(const char* Name) : TestName(Name) {
        QuicTraceLogInfo(
            TestCaseStart,
            "[test] START %s",
            TestName);
    }
    ~TestLogger() {
        QuicTraceLogInfo(
            TestCaseEnd,
            "[test] END %s",
            TestName);
    }
};

template<class T>
struct TestLoggerT {
    const char* TestName;
    TestLoggerT(const char* Name, const T& Params) : TestName(Name) {
        std::ostringstream stream; stream << Params;
        QuicTraceLogInfo(
            TestCaseTStart,
            "[test] START %s, %s",
            TestName,
            stream.str().c_str());
    }
    ~TestLoggerT() {
        QuicTraceLogInfo(
            TestCaseTEnd,
            "[test] END %s",
            TestName);
    }
};

TEST(ParameterValidation, ValidateApi) {
    TestLogger Logger("QuicTestValidateApi");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_API));
    } else {
        QuicTestValidateApi();
    }
}

TEST(ParameterValidation, ValidateRegistration) {
    TestLogger Logger("QuicTestValidateRegistration");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_REGISTRATION));
    } else {
        QuicTestValidateRegistration();
    }
}

TEST(ParameterValidation, ValidateGetPerfCounters) {
    TestLogger Logger("QuicTestGetPerfCounters");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_GET_PERF_COUNTERS));
    } else {
        QuicTestGetPerfCounters();
    }
}

TEST(ParameterValidation, ValidateConfiguration) {
    TestLogger Logger("QuicTestValidateConfiguration");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_CONFIGURATION));
    } else {
        QuicTestValidateConfiguration();
    }
}

TEST(ParameterValidation, ValidateListener) {
    TestLogger Logger("QuicTestValidateListener");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_LISTENER));
    } else {
        QuicTestValidateListener();
    }
}

TEST(ParameterValidation, ValidateConnection) {
    TestLogger Logger("QuicTestValidateConnection");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_CONNECTION));
    } else {
        QuicTestValidateConnection();
    }
}

TEST_P(WithBool, ValidateStream) {
    TestLoggerT<ParamType> Logger("QuicTestValidateStream", GetParam());
    if (TestingKernelMode) {
        uint8_t Param = (uint8_t)GetParam();
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_STREAM, Param));
    } else {
        QuicTestValidateStream(GetParam());
    }
}

TEST_P(WithValidateConnectionEventArgs, ValidateConnectionEvents) {
    TestLoggerT<ParamType> Logger("QuicTestValidateConnectionEvents", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run<uint32_t>(IOCTL_QUIC_RUN_VALIDATE_CONNECTION_EVENTS, GetParam().Test));
    } else {
        QuicTestValidateConnectionEvents(GetParam().Test);
    }
}

TEST_P(WithValidateStreamEventArgs, ValidateStreamEvents) {
    TestLoggerT<ParamType> Logger("QuicTestValidateStreamEvents", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run<uint32_t>(IOCTL_QUIC_RUN_VALIDATE_STREAM_EVENTS, GetParam().Test));
    } else {
        QuicTestValidateStreamEvents(GetParam().Test);
    }
}

TEST(ParameterValidation, ValidateDesiredVersionSettings) {
    TestLogger Logger("QuicTestDesiredVersionSettings");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_DESIRED_VERSIONS_SETTINGS));
    } else {
        QuicTestDesiredVersionSettings();
    }
}

TEST(ParameterValidation, ValidateParamApi) {
    TestLogger Logger("QuicTestValidateParamApi");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALIDATE_PARAM_API));
    } else {
        QuicTestValidateParamApi();
    }
}

TEST(Basic, CreateListener) {
    TestLogger Logger("QuicTestCreateListener");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CREATE_LISTENER));
    } else {
        QuicTestCreateListener();
    }
}

TEST(Basic, StartListener) {
    TestLogger Logger("QuicTestStartListener");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_LISTENER));
    } else {
        QuicTestStartListener();
    }
}

TEST(Basic, StartListenerMultiAlpns) {
    TestLogger Logger("QuicTestStartListenerMultiAlpns");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_LISTENER_MULTI_ALPN));
    } else {
        QuicTestStartListenerMultiAlpns();
    }
}

TEST_P(WithFamilyArgs, StartListenerImplicit) {
    TestLoggerT<ParamType> Logger("QuicTestStartListenerImplicit", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_LISTENER_IMPLICIT, GetParam().Family));
    } else {
        QuicTestStartListenerImplicit(GetParam().Family);
    }
}

TEST(Basic, StartTwoListeners) {
    TestLogger Logger("QuicTestStartTwoListeners");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_TWO_LISTENERS));
    } else {
        QuicTestStartTwoListeners();
    }
}

TEST(Basic, StartTwoListenersSameALPN) {
    TestLogger Logger("QuicTestStartTwoListenersSameALPN");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_TWO_LISTENERS_SAME_ALPN));
    } else {
        QuicTestStartTwoListenersSameALPN();
    }
}

TEST_P(WithFamilyArgs, StartListenerExplicit) {
    TestLoggerT<ParamType> Logger("QuicTestStartListenerExplicit", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_START_LISTENER_EXPLICIT, GetParam().Family));
    } else {
        QuicTestStartListenerExplicit(GetParam().Family);
    }
}

TEST(Basic, CreateConnection) {
    TestLogger Logger("QuicTestCreateConnection");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CREATE_CONNECTION));
    } else {
        QuicTestCreateConnection();
    }
}

#ifdef QUIC_TEST_DATAPATH_HOOKS_ENABLED

TEST(Mtu, Settings) {
    TestLogger Logger("QuicTestMtuSettings");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_MTU_SETTINGS));
    } else {
        QuicTestMtuSettings();
    }
}

TEST_P(WithMtuArgs, MtuDiscovery) {
    TestLoggerT<ParamType> Logger("QuicTestMtuDiscovery", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_MTU_DISCOVERY_PARAMS Params = {
            GetParam().Family,
            (uint8_t)(GetParam().DropMode & 1),
            (uint8_t)(GetParam().DropMode & 2),
            (uint8_t)GetParam().RaiseMinimum
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_MTU_DISCOVERY, Params));
    }
    else {
        QuicTestMtuDiscovery(
            GetParam().Family,
            GetParam().DropMode & 1,
            GetParam().DropMode & 2,
            GetParam().RaiseMinimum);
    }
}

#endif // QUIC_TEST_DATAPATH_HOOKS_ENABLED

TEST(Alpn, ValidAlpnLengths) {
    TestLogger Logger("QuicTestValidAlpnLengths");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALID_ALPN_LENGTHS));
    } else {
        QuicTestValidAlpnLengths();
    }
}

TEST(Alpn, InvalidAlpnLengths) {
    TestLogger Logger("QuicTestInvalidAlpnLengths");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_INVALID_ALPN_LENGTHS));
    } else {
        QuicTestInvalidAlpnLengths();
    }
}


TEST_P(WithFamilyArgs, BindConnectionImplicit) {
    TestLoggerT<ParamType> Logger("QuicTestBindConnectionImplicit", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_BIND_CONNECTION_IMPLICIT, GetParam().Family));
    } else {
        QuicTestBindConnectionImplicit(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, BindConnectionExplicit) {
    TestLoggerT<ParamType> Logger("QuicTestBindConnectionExplicit", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_BIND_CONNECTION_EXPLICIT, GetParam().Family));
    } else {
        QuicTestBindConnectionExplicit(GetParam().Family);
    }
}

TEST_P(WithHandshakeArgs1, Connect) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-Connect", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            (uint8_t)GetParam().MultipleALPNs,
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_DISABLED,
            0   // RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            GetParam().MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_DISABLED,
            0);     // RandomLossPercentage
    }
}

#ifndef QUIC_DISABLE_RESUMPTION
TEST_P(WithHandshakeArgs1, Resume) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-Resume", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            (uint8_t)GetParam().MultipleALPNs,
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_ENABLED,
            0   // RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            GetParam().MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_ENABLED,
            0);     // RandomLossPercentage
    }
}

TEST_P(WithHandshakeArgs1, ResumeRejection) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-ResumeRejection", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            (uint8_t)GetParam().MultipleALPNs,
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_REJECTED,
            0   // RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            GetParam().MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_REJECTED,
            0);     // RandomLossPercentage
    }
}
#endif // QUIC_DISABLE_RESUMPTION

#ifndef QUIC_DISABLE_SHARED_PORT_TESTS
TEST_P(WithFamilyArgs, ClientSharedLocalPort) {
    TestLoggerT<ParamType> Logger("QuicTestClientSharedLocalPort", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CLIENT_SHARED_LOCAL_PORT, GetParam().Family));
    } else {
        QuicTestClientSharedLocalPort(GetParam().Family);
    }
}
#endif

TEST_P(WithHandshakeArgs2, OldVersion) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-OldVersion", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            1,  // ClientUsesOldVersion
            0,  // MultipleALPNs
            0,  // AsyncConfiguration
            0,  // MultiPacketClientInitial
            QUIC_TEST_RESUMPTION_DISABLED,  // SessionResumption
            0   // RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            false,  // MultipleALPNs
            false,  // AsyncConfiguration
            false,  // MultiPacketClientInitial
            QUIC_TEST_RESUMPTION_DISABLED,  // SessionResumption
            0);     // RandomLossPercentage
    }
}

TEST_P(WithHandshakeArgs3, AsyncSecurityConfig) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-AsyncSecurityConfig", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            (uint8_t)GetParam().MultipleALPNs,
            1,  // AsyncConfiguration
            0,  // MultiPacketClientInitial
            QUIC_TEST_RESUMPTION_DISABLED,  // SessionResumption
            0   // RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            GetParam().MultipleALPNs,
            true,   // AsyncConfiguration
            false,  // MultiPacketClientInitial
            QUIC_TEST_RESUMPTION_DISABLED,  // SessionResumption
            0);     // RandomLossPercentage
    }
}

TEST_P(WithFamilyArgs, VersionNegotiation) {
    TestLoggerT<ParamType> Logger("QuicTestVersionNegotiation", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VERSION_NEGOTIATION, GetParam().Family));
    } else {
        QuicTestVersionNegotiation(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, VersionNegotiationRetry) {
    TestLoggerT<ParamType> Logger("QuicTestVersionNegotiationRetry", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VERSION_NEGOTIATION_RETRY, GetParam().Family));
    } else {
        QuicTestVersionNegotiationRetry(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, CompatibleVersionNegotiationRetry) {
    TestLoggerT<ParamType> Logger("CompatibleVersionNegotiationRetry", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_COMPATIBLE_VERSION_NEGOTIATION_RETRY, GetParam().Family));
    } else {
        QuicTestCompatibleVersionNegotiationRetry(GetParam().Family);
    }
}

TEST_P(WithVersionNegotiationExtArgs, CompatibleVersionNegotiation) {
    TestLoggerT<ParamType> Logger("CompatibleVersionNegotiation", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_VERSION_NEGOTIATION_EXT Params = {
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_COMPATIBLE_VERSION_NEGOTIATION, Params));
    } else {
        QuicTestCompatibleVersionNegotiation(
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer);
    }
}

TEST_P(WithVersionNegotiationExtArgs, CompatibleVersionNegotiationDefaultServer) {
    TestLoggerT<ParamType> Logger("CompatibleVersionNegotiationDefaultServer", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_VERSION_NEGOTIATION_EXT Params = {
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_COMPATIBLE_VERSION_NEGOTIATION_DEFAULT_SERVER, Params));
    } else {
        QuicTestCompatibleVersionNegotiationDefaultServer(
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer);
    }
}

TEST_P(WithVersionNegotiationExtArgs, CompatibleVersionNegotiationDefaultClient) {
    TestLoggerT<ParamType> Logger("CompatibleVersionNegotiationDefaultClient", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_VERSION_NEGOTIATION_EXT Params = {
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_COMPATIBLE_VERSION_NEGOTIATION_DEFAULT_CLIENT, Params));
    } else {
        QuicTestCompatibleVersionNegotiationDefaultClient(
            GetParam().Family,
            GetParam().DisableVNEClient,
            GetParam().DisableVNEServer);
    }
}

TEST_P(WithFamilyArgs, IncompatibleVersionNegotiation) {
    TestLoggerT<ParamType> Logger("IncompatibleVersionNegotiation", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_INCOMPATIBLE_VERSION_NEGOTIATION, GetParam().Family));
    } else {
        QuicTestIncompatibleVersionNegotiation(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, FailedVersionNegotiation) {
    TestLoggerT<ParamType> Logger("FailedeVersionNegotiation", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_FAILED_VERSION_NEGOTIATION, GetParam().Family));
    } else {
        QuicTestFailedVersionNegotiation(GetParam().Family);
    }
}

TEST_P(WithHandshakeArgs5, CustomCertificateValidation) {
    TestLoggerT<ParamType> Logger("QuicTestCustomCertificateValidation", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CUSTOM_CERT_VALIDATION Params = {
            GetParam().AcceptCert,
            GetParam().AsyncValidation
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CUSTOM_CERT_VALIDATION, Params));
    } else {
        QuicTestCustomCertificateValidation(GetParam().AcceptCert, GetParam().AsyncValidation);
    }
}

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
TEST_P(WithHandshakeArgs6, ConnectClientCertificate) {
    TestLoggerT<ParamType> Logger("QuicTestConnectClientCertificate", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_CLIENT_CERT Params = {
            GetParam().Family,
            (uint8_t)GetParam().UseClientCertificate
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_CLIENT_CERT, Params));
    } else {
        QuicTestConnectClientCertificate(GetParam().Family, GetParam().UseClientCertificate);
    }
}
#endif

#if QUIC_TEST_FAILING_TEST_CERTIFICATES
TEST(CredValidation, ConnectExpiredServerCertificate) {
    QUIC_RUN_CRED_VALIDATION Params;
    for (auto CredType : { QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH, QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE }) {
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_EXPIRED_SERVER,
            TestingKernelMode ?
                CXPLAT_SELF_SIGN_CERT_MACHINE :
                CXPLAT_SELF_SIGN_CERT_USER,
            CredType,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        if (TestingKernelMode) {
            ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_EXPIRED_SERVER_CERT, Params));
        } else {
            QuicTestConnectExpiredServerCertificate(&Params.CredConfig);
        }
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }


    if (!TestingKernelMode) {
        //
        // Test cert context in user mode only.
        //
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_EXPIRED_SERVER,
            CXPLAT_SELF_SIGN_CERT_USER,
            QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        QuicTestConnectExpiredServerCertificate(&Params.CredConfig);
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }
}

TEST(CredValidation, ConnectValidServerCertificate) {
    QUIC_RUN_CRED_VALIDATION Params;
    for (auto CredType : { QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH, QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE }) {
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_VALID_SERVER,
            TestingKernelMode ?
                CXPLAT_SELF_SIGN_CERT_MACHINE :
                CXPLAT_SELF_SIGN_CERT_USER,
            CredType,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        if (TestingKernelMode) {
            ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALID_SERVER_CERT, Params));
        } else {
            QuicTestConnectValidServerCertificate(&Params.CredConfig);
        }
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }

    if (!TestingKernelMode) {
        //
        // Test cert context in user mode only.
        //
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_VALID_SERVER,
            CXPLAT_SELF_SIGN_CERT_USER,
            QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        QuicTestConnectValidServerCertificate(&Params.CredConfig);
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }
}

TEST(CredValidation, ConnectExpiredClientCertificate) {
    QUIC_RUN_CRED_VALIDATION Params;
    for (auto CredType : { QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH, QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE }) {
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_EXPIRED_CLIENT,
            TestingKernelMode ?
                CXPLAT_SELF_SIGN_CERT_MACHINE :
                CXPLAT_SELF_SIGN_CERT_USER,
            CredType,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        Params.CredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

        if (TestingKernelMode) {
            ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_EXPIRED_CLIENT_CERT, Params));
        } else {
            QuicTestConnectExpiredClientCertificate(&Params.CredConfig);
        }
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }

    if (!TestingKernelMode) {
        //
        // Test cert context in user mode only.
        //
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_EXPIRED_CLIENT,
            CXPLAT_SELF_SIGN_CERT_USER,
            QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        Params.CredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
        QuicTestConnectExpiredClientCertificate(&Params.CredConfig);
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }
}

TEST(CredValidation, ConnectValidClientCertificate) {
    QUIC_RUN_CRED_VALIDATION Params;
    for (auto CredType : { QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH, QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE }) {
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_VALID_CLIENT,
            TestingKernelMode ?
                CXPLAT_SELF_SIGN_CERT_MACHINE :
                CXPLAT_SELF_SIGN_CERT_USER,
            CredType,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        Params.CredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

        if (TestingKernelMode) {
            ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_VALID_CLIENT_CERT, Params));
        } else {
            QuicTestConnectValidClientCertificate(&Params.CredConfig);
        }
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }

    if (!TestingKernelMode) {
        //
        // Test cert context in user mode only.
        //
        ASSERT_TRUE(CxPlatGetTestCertificate(
            CXPLAT_TEST_CERT_VALID_CLIENT,
            CXPLAT_SELF_SIGN_CERT_USER,
            QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT,
            &Params.CredConfig,
            &Params.CertHash,
            &Params.CertHashStore,
            (char*)Params.PrincipalString));
        Params.CredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
        QuicTestConnectValidClientCertificate(&Params.CredConfig);
        CxPlatFreeTestCert((QUIC_CREDENTIAL_CONFIG*)&Params.CredConfig);
    }
}
#endif // QUIC_TEST_FAILING_TEST_CERTIFICATES

#if QUIC_TEST_DATAPATH_HOOKS_ENABLED
TEST_P(WithHandshakeArgs4, RandomLoss) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-RandomLoss", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            0,  // MultipleALPNs
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_DISABLED,
            GetParam().RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            false,  // MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_DISABLED,
            GetParam().RandomLossPercentage);
    }
}
#ifndef QUIC_DISABLE_RESUMPTION
TEST_P(WithHandshakeArgs4, RandomLossResume) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-RandomLossResume", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            0,  // MultipleALPNs
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_ENABLED,
            GetParam().RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            false,  // MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_ENABLED,
            GetParam().RandomLossPercentage);
    }
}
TEST_P(WithHandshakeArgs4, RandomLossResumeRejection) {
    TestLoggerT<ParamType> Logger("QuicTestConnect-RandomLossResumeRejection", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().ServerStatelessRetry,
            0,  // ClientUsesOldVersion
            0,  // MultipleALPNs
            0,  // AsyncConfiguration
            (uint8_t)GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_REJECTED,
            GetParam().RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT, Params));
    } else {
        QuicTestConnect(
            GetParam().Family,
            GetParam().ServerStatelessRetry,
            false,  // ClientUsesOldVersion
            false,  // MultipleALPNs,
            false,  // AsyncConfiguration
            GetParam().MultiPacketClientInitial,
            QUIC_TEST_RESUMPTION_REJECTED,
            GetParam().RandomLossPercentage);
    }
}
#endif // QUIC_DISABLE_RESUMPTION
#endif // QUIC_TEST_DATAPATH_HOOKS_ENABLED

TEST_P(WithFamilyArgs, Unreachable) {
    TestLoggerT<ParamType> Logger("QuicTestConnectUnreachable", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_UNREACHABLE, GetParam().Family));
    } else {
        QuicTestConnectUnreachable(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, BadALPN) {
    TestLoggerT<ParamType> Logger("QuicTestConnectBadAlpn", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_BAD_ALPN, GetParam().Family));
    } else {
        QuicTestConnectBadAlpn(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, BadSNI) {
    TestLoggerT<ParamType> Logger("QuicTestConnectBadSni", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_BAD_SNI, GetParam().Family));
    } else {
        QuicTestConnectBadSni(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, ServerRejected) {
    TestLoggerT<ParamType> Logger("QuicTestConnectServerRejected", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_SERVER_REJECTED, GetParam().Family));
    } else {
        QuicTestConnectServerRejected(GetParam().Family);
    }
}

#if QUIC_TEST_DATAPATH_HOOKS_ENABLED
TEST_P(WithFamilyArgs, RebindPort) {
    TestLoggerT<ParamType> Logger("QuicTestNatPortRebind", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_NAT_PORT_REBIND, GetParam().Family));
    } else {
        QuicTestNatPortRebind(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, RebindAddr) {
    TestLoggerT<ParamType> Logger("QuicTestNatAddrRebind", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_NAT_ADDR_REBIND, GetParam().Family));
    } else {
        QuicTestNatAddrRebind(GetParam().Family);
    }
}

TEST_P(WithFamilyArgs, PathValidationTimeout) {
    TestLoggerT<ParamType> Logger("QuicTestPathValidationTimeout", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_PATH_VALIDATION_TIMEOUT, GetParam().Family));
    } else {
        QuicTestPathValidationTimeout(GetParam().Family);
    }
}
#endif

TEST_P(WithFamilyArgs, ChangeMaxStreamIDs) {
    TestLoggerT<ParamType> Logger("QuicTestChangeMaxStreamID", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CHANGE_MAX_STREAM_ID, GetParam().Family));
    } else {
        QuicTestChangeMaxStreamID(GetParam().Family);
    }
}

#if QUIC_TEST_DATAPATH_HOOKS_ENABLED
TEST_P(WithFamilyArgs, LoadBalanced) {
    TestLoggerT<ParamType> Logger("QuicTestLoadBalancedHandshake", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_LOAD_BALANCED_HANDSHAKE, GetParam().Family));
    } else {
        QuicTestLoadBalancedHandshake(GetParam().Family);
    }
}
#endif // QUIC_TEST_DATAPATH_HOOKS_ENABLED

TEST_P(WithSendArgs1, Send) {
    TestLoggerT<ParamType> Logger("QuicTestConnectAndPing", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_AND_PING_PARAMS Params = {
            GetParam().Family,
            GetParam().Length,
            GetParam().ConnectionCount,
            GetParam().StreamCount,
            1,  // StreamBurstCount
            0,  // StreamBurstDelayMs
            0,  // ServerStatelessRetry
            0,  // ClientRebind
            0,  // ClientZeroRtt
            0,  // ServerRejectZeroRtt
            (uint8_t)GetParam().UseSendBuffer,
            (uint8_t)GetParam().UnidirectionalStreams,
            (uint8_t)GetParam().ServerInitiatedStreams,
            0   // FifoScheduling
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_PING, Params));
    } else {
        QuicTestConnectAndPing(
            GetParam().Family,
            GetParam().Length,
            GetParam().ConnectionCount,
            GetParam().StreamCount,
            1,      // StreamBurstCount
            0,      // StreamBurstDelayMs
            false,  // ServerStatelessRetry
            false,  // ClientRebind
            false,  // ClientZeroRtt
            false,  // ServerRejectZeroRtt
            GetParam().UseSendBuffer,
            GetParam().UnidirectionalStreams,
            GetParam().ServerInitiatedStreams,
            false); // FifoScheduling
    }
}

TEST_P(WithSendArgs2, SendLarge) {
    TestLoggerT<ParamType> Logger("QuicTestConnectAndPing", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_AND_PING_PARAMS Params = {
            GetParam().Family,
            100000000llu,
            1,  // ConnectionCount
            1,  // StreamCount
            1,  // StreamBurstCount
            0,  // StreamBurstDelayMs
            0,  // ServerStatelessRetry
            0,  // ClientRebind
            (uint8_t)GetParam().UseZeroRtt,
            0,  // ServerRejectZeroRtt
            (uint8_t)GetParam().UseSendBuffer,
            0,  // UnidirectionalStreams
            0,  // ServerInitiatedStreams
            1   // FifoScheduling
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_PING, Params));
    } else {
        QuicTestConnectAndPing(
            GetParam().Family,
            100000000llu,
            1,      // ConnectionCount
            1,      // StreamCount
            1,      // StreamBurstCount
            0,      // StreamBurstDelayMs
            false,  // ServerStatelessRetry
            false,  // ClientRebind
            GetParam().UseZeroRtt,
            false,  // ServerRejectZeroRtt
            GetParam().UseSendBuffer,
            false,  // UnidirectionalStreams
            false,  // ServerInitiatedStreams
            true);  // FifoScheduling
    }
}

TEST_P(WithSendArgs3, SendIntermittently) {
    TestLoggerT<ParamType> Logger("QuicTestConnectAndPing", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_AND_PING_PARAMS Params = {
            GetParam().Family,
            GetParam().Length,
            1,  // ConnectionCount
            1,  // StreamCount
            GetParam().BurstCount,
            GetParam().BurstDelay,
            0,  // ServerStatelessRetry
            0,  // ClientRebind
            0,  // ClientZeroRtt
            0,  // ServerRejectZeroRtt
            (uint8_t)GetParam().UseSendBuffer,
            0,  // UnidirectionalStreams
            0,  // ServerInitiatedStreams
            0   // FifoScheduling
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_PING, Params));
    } else {
        QuicTestConnectAndPing(
            GetParam().Family,
            GetParam().Length,
            1,  // ConnectionCount
            1,  // StreamCount
            GetParam().BurstCount,
            GetParam().BurstDelay,
            false,  // ServerStatelessRetry
            false,  // ClientRebind
            false,  // ClientZeroRtt
            false,  // ServerRejectZeroRtt
            GetParam().UseSendBuffer,
            false,  // UnidirectionalStreams
            false,  // ServerInitiatedStreams
            false); // FifoScheduling
    }
}

#ifndef QUIC_DISABLE_0RTT_TESTS

TEST_P(WithSend0RttArgs1, Send0Rtt) {
    TestLoggerT<ParamType> Logger("Send0Rtt", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_AND_PING_PARAMS Params = {
            GetParam().Family,
            GetParam().Length,
            GetParam().ConnectionCount,
            GetParam().StreamCount,
            1,  // StreamBurstCount
            0,  // StreamBurstDelayMs
            0,  // ServerStatelessRetry
            0,  // ClientRebind
            1,  // ClientZeroRtt,
            0,  // ServerRejectZeroRtt
            (uint8_t)GetParam().UseSendBuffer,
            (uint8_t)GetParam().UnidirectionalStreams,
            0,  // ServerInitiatedStreams
            0   // FifoScheduling
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_PING, Params));
    } else {
        QuicTestConnectAndPing(
            GetParam().Family,
            GetParam().Length,
            GetParam().ConnectionCount,
            GetParam().StreamCount,
            1,      // StreamBurstCount
            0,      // StreamBurstDelayMs
            false,  // ServerStatelessRetry
            false,  // ClientRebind
            true,   // ClientZeroRtt
            false,  // ServerRejectZeroRtt
            GetParam().UseSendBuffer,
            GetParam().UnidirectionalStreams,
            false,  // ServerInitiatedStreams
            false); // FifoScheduling
    }
}

TEST_P(WithSend0RttArgs2, Reject0Rtt) {
    TestLoggerT<ParamType> Logger("Reject0Rtt", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CONNECT_AND_PING_PARAMS Params = {
            GetParam().Family,
            GetParam().Length,
            1,  // StreamCount
            1,  // StreamBurstCount
            1,  // StreamBurstCount
            0,  // StreamBurstDelayMs
            0,  // ServerStatelessRetry
            0,  // ClientRebind
            1,  // ClientZeroRtt,
            1,  // ServerRejectZeroRtt
            0,  // UseSendBuffer
            0,  // UnidirectionalStreams
            0,  // ServerInitiatedStreams
            0   // FifoScheduling
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_PING, Params));
    } else {
        QuicTestConnectAndPing(
            GetParam().Family,
            GetParam().Length,
            1,      // StreamCount
            1,      // StreamBurstCount
            1,      // StreamBurstCount
            0,      // StreamBurstDelayMs
            false,  // ServerStatelessRetry
            false,  // ClientRebind
            true,   // ClientZeroRtt
            true,   // ServerRejectZeroRtt
            false,  // UseSendBuffer
            false,  // UnidirectionalStreams
            false,  // ServerInitiatedStreams
            false); // FifoScheduling
    }
}

#endif // QUIC_DISABLE_0RTT_TESTS

TEST_P(WithBool, IdleTimeout) {
    TestLoggerT<ParamType> Logger("QuicTestConnectAndIdle", GetParam());
    if (TestingKernelMode) {
        uint8_t Param = (uint8_t)GetParam();
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CONNECT_AND_IDLE, Param));
    } else {
        QuicTestConnectAndIdle(GetParam());
    }
}

TEST(Misc, ServerDisconnect) {
    TestLogger Logger("QuicTestServerDisconnect");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_SERVER_DISCONNECT));
    } else {
        QuicTestServerDisconnect();
    }
}

TEST(Misc, ClientDisconnect) {
    TestLogger Logger("QuicTestClientDisconnect");
    if (TestingKernelMode) {
        uint8_t Param = 0;
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CLIENT_DISCONNECT, Param));
    } else {
        QuicTestClientDisconnect(false); // TODO - Support true, when race condition is fixed.
    }
}

TEST_P(WithKeyUpdateArgs1, KeyUpdate) {
    TestLoggerT<ParamType> Logger("QuicTestKeyUpdate", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_KEY_UPDATE_PARAMS Params = {
            GetParam().Family,
            (uint16_t)(GetParam().KeyUpdate == 0 ? 5 : 1),  // Iterations
            0,                                              // KeyUpdateBytes
            (uint8_t)(GetParam().KeyUpdate == 0),           // UseKeyUpdateBytes
            (uint8_t)(GetParam().KeyUpdate & 1),            // ClientKeyUpdate
            (uint8_t)(GetParam().KeyUpdate & 2)             // ServerKeyUpdate
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_KEY_UPDATE, Params));
    } else {
        QuicTestKeyUpdate(
            GetParam().Family,
            GetParam().KeyUpdate == 0 ? 5 : 1,  // Iterations
            0,                                  // KeyUpdateBytes
            GetParam().KeyUpdate == 0,          // UseKeyUpdateBytes
            GetParam().KeyUpdate & 1,           // ClientKeyUpdate
            GetParam().KeyUpdate & 2);          // ServerKeyUpdate
    }
}

#if QUIC_TEST_DATAPATH_HOOKS_ENABLED
TEST_P(WithKeyUpdateArgs2, RandomLoss) {
    TestLoggerT<ParamType> Logger("QuicTestKeyUpdateRandomLoss", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_KEY_UPDATE_RANDOM_LOSS_PARAMS Params = {
            GetParam().Family,
            GetParam().RandomLossPercentage
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_KEY_UPDATE_RANDOM_LOSS, Params));
    } else {
        QuicTestKeyUpdateRandomLoss(
            GetParam().Family,
            GetParam().RandomLossPercentage);
    }
}
#endif

TEST_P(WithAbortiveArgs, AbortiveShutdown) {
    TestLoggerT<ParamType> Logger("QuicAbortiveTransfers", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_ABORTIVE_SHUTDOWN_PARAMS Params = {
            GetParam().Family,
            GetParam().Flags
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_ABORTIVE_SHUTDOWN, Params));
    } else {
        QuicAbortiveTransfers(GetParam().Family, GetParam().Flags);
    }
}

TEST_P(WithCidUpdateArgs, CidUpdate) {
    TestLoggerT<ParamType> Logger("QuicTestCidUpdate", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_CID_UPDATE_PARAMS Params = {
            GetParam().Family,
            GetParam().Iterations
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_CID_UPDATE, Params));
    } else {
        QuicTestCidUpdate(GetParam().Family, GetParam().Iterations);
    }
}

TEST_P(WithReceiveResumeArgs, ReceiveResume) {
    TestLoggerT<ParamType> Logger("QuicTestReceiveResume", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_RECEIVE_RESUME_PARAMS Params = {
            GetParam().Family,
            GetParam().SendBytes,
            GetParam().ConsumeBytes,
            GetParam().ShutdownType,
            GetParam().PauseType,
            (uint8_t)GetParam().PauseFirst
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_RECEIVE_RESUME, Params));
    } else {
        QuicTestReceiveResume(
            GetParam().Family,
            GetParam().SendBytes,
            GetParam().ConsumeBytes,
            GetParam().ShutdownType,
            GetParam().PauseType,
            GetParam().PauseFirst);
    }
}

TEST_P(WithReceiveResumeNoDataArgs, ReceiveResumeNoData) {
    TestLoggerT<ParamType> Logger("QuicTestReceiveResumeNoData", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_RECEIVE_RESUME_PARAMS Params = {
            GetParam().Family,
            0,
            0,
            GetParam().ShutdownType,
            ReturnConsumedBytes,
            0
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_RECEIVE_RESUME_NO_DATA, Params));
    } else {
        QuicTestReceiveResumeNoData(GetParam().Family, GetParam().ShutdownType);
    }
}

TEST_P(WithFamilyArgs, AckSendDelay) {
    TestLogger Logger("QuicTestAckSendDelay");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_ACK_SEND_DELAY, GetParam().Family));
    } else {
        QuicTestAckSendDelay(GetParam().Family);
    }
}

TEST(Misc, AbortPausedReceive) {
    TestLogger Logger("AbortPausedReceive");
    if (TestingKernelMode) {
        QUIC_ABORT_RECEIVE_TYPE Type = QUIC_ABORT_RECEIVE_PAUSED;
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_ABORT_RECEIVE, Type));
    } else {
        QuicTestAbortReceive(QUIC_ABORT_RECEIVE_PAUSED);
    }
}

TEST(Misc, AbortPendingReceive) {
    TestLogger Logger("AbortPendingReceive");
    if (TestingKernelMode) {
        QUIC_ABORT_RECEIVE_TYPE Type = QUIC_ABORT_RECEIVE_PENDING;
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_ABORT_RECEIVE, Type));
    } else {
        QuicTestAbortReceive(QUIC_ABORT_RECEIVE_PENDING);
    }
}

TEST(Misc, AbortIncompleteReceive) {
    TestLogger Logger("AbortIncompleteReceive");
    if (TestingKernelMode) {
        QUIC_ABORT_RECEIVE_TYPE Type = QUIC_ABORT_RECEIVE_INCOMPLETE;
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_ABORT_RECEIVE, Type));
    } else {
        QuicTestAbortReceive(QUIC_ABORT_RECEIVE_INCOMPLETE);
    }
}

TEST(Misc, SlowReceive) {
    TestLogger Logger("SlowReceive");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_SLOW_RECEIVE));
    } else {
        QuicTestSlowReceive();
    }
}

#ifdef QUIC_TEST_ALLOC_FAILURES_ENABLED
TEST(Misc, NthAllocFail) {
    TestLogger Logger("NthAllocFail");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_NTH_ALLOC_FAIL));
    } else {
        QuicTestNthAllocFail();
    }
}
#endif

TEST(Misc, StreamPriority) {
    TestLogger Logger("StreamPriority");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_STREAM_PRIORITY));
    } else {
        QuicTestStreamPriority();
    }
}

TEST(Drill, VarIntEncoder) {
    TestLogger Logger("QuicDrillTestVarIntEncoder");
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_DRILL_ENCODE_VAR_INT));
    } else {
        QuicDrillTestVarIntEncoder();
    }
}

TEST_P(WithDrillInitialPacketCidArgs, DrillInitialPacketCids) {
    TestLoggerT<ParamType> Logger("QuicDrillInitialPacketCids", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_DRILL_INITIAL_PACKET_CID_PARAMS Params = {
            GetParam().Family,
            (uint8_t)GetParam().SourceOrDest,
            (uint8_t)GetParam().ActualCidLengthValid,
            (uint8_t)GetParam().ShortCidLength,
            (uint8_t)GetParam().CidLengthFieldValid
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_DRILL_INITIAL_PACKET_CID, Params));
    } else {
        QuicDrillTestInitialCid(
            GetParam().Family,
            GetParam().SourceOrDest,
            GetParam().ActualCidLengthValid,
            GetParam().ShortCidLength,
            GetParam().CidLengthFieldValid);
    }
}

TEST_P(WithDrillInitialPacketTokenArgs, DrillInitialPacketToken) {
    TestLoggerT<ParamType> Logger("QuicDrillInitialPacketToken", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_DRILL_INITIAL_PACKET_TOKEN, GetParam().Family));
    } else {
        QuicDrillTestInitialToken(GetParam().Family);
    }
}

TEST_P(WithDatagramNegotiationArgs, DatagramNegotiation) {
    TestLoggerT<ParamType> Logger("QuicTestDatagramNegotiation", GetParam());
    if (TestingKernelMode) {
        QUIC_RUN_DATAGRAM_NEGOTIATION Params = {
            GetParam().Family,
            GetParam().DatagramReceiveEnabled
        };
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_DATAGRAM_NEGOTIATION, Params));
    } else {
        QuicTestDatagramNegotiation(GetParam().Family, GetParam().DatagramReceiveEnabled);
    }
}

TEST_P(WithFamilyArgs, DatagramSend) {
    TestLoggerT<ParamType> Logger("QuicTestDatagramSend", GetParam());
    if (TestingKernelMode) {
        ASSERT_TRUE(DriverClient.Run(IOCTL_QUIC_RUN_DATAGRAM_SEND, GetParam().Family));
    } else {
        QuicTestDatagramSend(GetParam().Family);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParameterValidation,
    WithBool,
    ::testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(
    ParameterValidation,
    WithValidateConnectionEventArgs,
    testing::ValuesIn(ValidateConnectionEventArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    ParameterValidation,
    WithValidateStreamEventArgs,
    testing::ValuesIn(ValidateStreamEventArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Basic,
    WithFamilyArgs,
    ::testing::ValuesIn(FamilyArgs::Generate()));

#ifdef QUIC_TEST_DATAPATH_HOOKS_ENABLED

INSTANTIATE_TEST_SUITE_P(
    Mtu,
    WithMtuArgs,
    ::testing::ValuesIn(MtuArgs::Generate()));

#endif // QUIC_TEST_DATAPATH_HOOKS_ENABLED

INSTANTIATE_TEST_SUITE_P(
    Basic,
    WithVersionNegotiationExtArgs,
    testing::ValuesIn(VersionNegotiationExtArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs1,
    testing::ValuesIn(HandshakeArgs1::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs2,
    testing::ValuesIn(HandshakeArgs2::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs3,
    testing::ValuesIn(HandshakeArgs3::Generate()));

#ifdef QUIC_TEST_DATAPATH_HOOKS_ENABLED

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs4,
    testing::ValuesIn(HandshakeArgs4::Generate()));

#endif

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs5,
    testing::ValuesIn(HandshakeArgs5::Generate()));

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS

INSTANTIATE_TEST_SUITE_P(
    Handshake,
    WithHandshakeArgs6,
    testing::ValuesIn(HandshakeArgs6::Generate()));

#endif

INSTANTIATE_TEST_SUITE_P(
    AppData,
    WithSendArgs1,
    testing::ValuesIn(SendArgs1::Generate()));

INSTANTIATE_TEST_SUITE_P(
    AppData,
    WithSendArgs2,
    testing::ValuesIn(SendArgs2::Generate()));

INSTANTIATE_TEST_SUITE_P(
    AppData,
    WithSendArgs3,
    testing::ValuesIn(SendArgs3::Generate()));

#ifndef QUIC_DISABLE_0RTT_TESTS

INSTANTIATE_TEST_SUITE_P(
    AppData,
    WithSend0RttArgs1,
    testing::ValuesIn(Send0RttArgs1::Generate()));

INSTANTIATE_TEST_SUITE_P(
    AppData,
    WithSend0RttArgs2,
    testing::ValuesIn(Send0RttArgs2::Generate()));

#endif

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithKeyUpdateArgs1,
    testing::ValuesIn(KeyUpdateArgs1::Generate()));

#if QUIC_TEST_DATAPATH_HOOKS_ENABLED

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithKeyUpdateArgs2,
    testing::ValuesIn(KeyUpdateArgs2::Generate()));

#endif

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithAbortiveArgs,
    testing::ValuesIn(AbortiveArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithCidUpdateArgs,
    testing::ValuesIn(CidUpdateArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithReceiveResumeArgs,
    testing::ValuesIn(ReceiveResumeArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithReceiveResumeNoDataArgs,
    testing::ValuesIn(ReceiveResumeNoDataArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Misc,
    WithDatagramNegotiationArgs,
    testing::ValuesIn(DatagramNegotiationArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Drill,
    WithDrillInitialPacketCidArgs,
    testing::ValuesIn(DrillInitialPacketCidArgs::Generate()));

INSTANTIATE_TEST_SUITE_P(
    Drill,
    WithDrillInitialPacketTokenArgs,
    testing::ValuesIn(DrillInitialPacketTokenArgs::Generate()));

int main(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp("--kernel", argv[i]) == 0 || strcmp("--kernelPriv", argv[i]) == 0) {
            TestingKernelMode = true;
            if (strcmp("--kernelPriv", argv[i]) == 0) {
                PrivateTestLibrary = true;
            }
        }
    }
    ::testing::AddGlobalTestEnvironment(new QuicTestEnvironment);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
