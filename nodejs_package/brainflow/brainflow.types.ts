export class BrainFlowError extends Error
{
    __proto__ = Error

        public exitCode: number;

    constructor(errorCode: number, msg: string)
    {
        super ("Error code is: " + errorCode + " " + msg);
        this.exitCode = errorCode;
        Object.setPrototypeOf(this, BrainFlowError.prototype);
    }
}

export enum BoardIds {
    NO_BOARD = -100,
    PLAYBACK_FILE_BOARD = -3,
    STREAMING_BOARD = -2,
    SYNTHETIC_BOARD = -1,
    CYTON_BOARD = 0,
    GANGLION_BOARD = 1,
    CYTON_DAISY_BOARD = 2,
    GALEA_BOARD = 3,
    GANGLION_WIFI_BOARD = 4,
    CYTON_WIFI_BOARD = 5,
    CYTON_DAISY_WIFI_BOARD = 6,
    BRAINBIT_BOARD = 7,
    UNICORN_BOARD = 8,
    CALLIBRI_EEG_BOARD = 9,
    CALLIBRI_EMG_BOARD = 10,
    CALLIBRI_ECG_BOARD = 11,
    NOTION_1_BOARD = 13,
    NOTION_2_BOARD = 14,
    GFORCE_PRO_BOARD = 16,
    FREEEEG32_BOARD = 17,
    BRAINBIT_BLED_BOARD = 18,
    GFORCE_DUAL_BOARD = 19,
    GALEA_SERIAL_BOARD = 20,
    MUSE_S_BLED_BOARD = 21,
    MUSE_2_BLED_BOARD = 22,
    CROWN_BOARD = 23,
    ANT_NEURO_EE_410_BOARD = 24,
    ANT_NEURO_EE_411_BOARD = 25,
    ANT_NEURO_EE_430_BOARD = 26,
    ANT_NEURO_EE_211_BOARD = 27,
    ANT_NEURO_EE_212_BOARD = 28,
    ANT_NEURO_EE_213_BOARD = 29,
    ANT_NEURO_EE_214_BOARD = 30,
    ANT_NEURO_EE_215_BOARD = 31,
    ANT_NEURO_EE_221_BOARD = 32,
    ANT_NEURO_EE_222_BOARD = 33,
    ANT_NEURO_EE_223_BOARD = 34,
    ANT_NEURO_EE_224_BOARD = 35,
    ANT_NEURO_EE_225_BOARD = 36,
    ENOPHONE_BOARD = 37,
    MUSE_2_BOARD = 38,
    MUSE_S_BOARD = 39,
    BRAINALIVE_BOARD = 40,
    MUSE_2016_BOARD = 41,
    MUSE_2016_BLED_BOARD = 42,
    EXPLORE_4_CHAN_BOARD = 44,
    EXPLORE_8_CHAN_BOARD = 45,
    GANGLION_NATIVE_BOARD = 46,
    EMOTIBIT_BOARD = 47,
    GALEA_BOARD_V4 = 48,
    GALEA_SERIAL_BOARD_V4 = 49,
    NTL_WIFI_BOARD = 50,
    ANT_NEURO_EE_511_BOARD = 51,
    EXPLORE_PLUS_8_CHAN_BOARD = 54,
    EXPLORE_PLUS_32_CHAN_BOARD = 55,
    PIEEG_BOARD = 56,
    NEUROPAWN_KNIGHT_BOARD = 57,
    SYNCHRONI_TRIO_3_CHANNELS_BOARD = 58,
    SYNCHRONI_OCTO_CHANNELS_BOARD = 59,
    OB5000_8_CHANNELS_BOARD = 60,
    SYNCHRONI_PENTO_8_CHANNELS_BOARD = 61,
    SYNCHRONI_UNO_1_CHANNELS_BOARD = 62,
    OB3000_24_CHANNELS_BOARD = 63,
    BIOLISTENER_BOARD = 64,
    CERELOG_X8_BOARD = 65
}

export enum IpProtocolTypes {
    NO_IP_PROTOCOL = 0,
    UDP = 1,
    TCP = 2,
}

export enum BrainFlowPresets {
    DEFAULT_PRESET = 0,
    AUXILIARY_PRESET = 1,
    ANCILLARY_PRESET = 2,
}

export enum LogLevels {
    LEVEL_TRACE = 0,
    LEVEL_DEBUG = 1,
    LEVEL_INFO = 2,
    LEVEL_WARN = 3,
    LEVEL_ERROR = 4,
    LEVEL_CRITICAL = 5,
    LEVEL_OFF = 6,
}

export enum FilterTypes {
    BUTTERWORTH = 0,
    CHEBYSHEV_TYPE_1 = 1,
    BESSEL = 2,
    BUTTERWORTH_ZERO_PHASE = 3,
    CHEBYSHEV_TYPE_1_ZERO_PHASE = 4,
    BESSEL_ZERO_PHASE = 5,
}

export enum AggOperations {
    MEAN = 0,
    MEDIAN = 1,
    EACH = 2,
}

export enum WindowOperations {
    NO_WINDOW = 0,
    HANNING = 1,
    HAMMING = 2,
    BLACKMAN_HARRIS = 3,
}

export enum DetrendOperations {
    NO_DETREND = 0,
    CONSTANT = 1,
    LINEAR = 2,
}

export enum NoiseTypes {
    FIFTY = 0,
    SIXTY = 1,
    FIFTY_AND_SIXTY = 2,
}

export enum WaveletDenoisingTypes {
    VISUSHRINK = 0,
    SURESHRINK = 1,
}

export enum ThresholdTypes {
    SOFT = 0,
    HARD = 1,
}

export enum WaveletExtensionTypes {
    SYMMETRIC = 0,
    PERIODIC = 1,
}

export enum NoiseEstimationLevelTypes {
    FIRST_LEVEL = 0,
    ALL_LEVELS = 1,
}

export enum WaveletTypes {
    HAAR = 0,
    DB1 = 1,
    DB2 = 2,
    DB3 = 3,
    DB4 = 4,
    DB5 = 5,
    DB6 = 6,
    DB7 = 7,
    DB8 = 8,
    DB9 = 9,
    DB10 = 10,
    DB11 = 11,
    DB12 = 12,
    DB13 = 13,
    DB14 = 14,
    DB15 = 15,
    BIOR1_1 = 16,
    BIOR1_3 = 17,
    BIOR1_5 = 18,
    BIOR2_2 = 19,
    BIOR2_4 = 20,
    BIOR2_6 = 21,
    BIOR2_8 = 22,
    BIOR3_1 = 23,
    BIOR3_3 = 24,
    BIOR3_5 = 25,
    BIOR3_7 = 26,
    BIOR3_9 = 27,
    BIOR4_4 = 28,
    BIOR5_5 = 29,
    BIOR6_8 = 30,
    COIF1 = 31,
    COIF2 = 32,
    COIF3 = 33,
    COIF4 = 34,
    COIF5 = 35,
    SYM2 = 36,
    SYM3 = 37,
    SYM4 = 38,
    SYM5 = 39,
    SYM6 = 40,
    SYM7 = 41,
    SYM8 = 42,
    SYM9 = 43,
    SYM10 = 44,
}

export enum BrainFlowExitCodes {
    STATUS_OK = 0,
    PORT_ALREADY_OPEN_ERROR = 1,
    UNABLE_TO_OPEN_PORT_ERROR = 2,
    SER_PORT_ERROR = 3,
    BOARD_WRITE_ERROR = 4,
    INCOMMING_MSG_ERROR = 5,
    INITIAL_MSG_ERROR = 6,
    BOARD_NOT_READY_ERROR = 7,
    STREAM_ALREADY_RUN_ERROR = 8,
    INVALID_BUFFER_SIZE_ERROR = 9,
    STREAM_THREAD_ERROR = 10,
    STREAM_THREAD_IS_NOT_RUNNING = 11,
    EMPTY_BUFFER_ERROR = 12,
    INVALID_ARGUMENTS_ERROR = 13,
    UNSUPPORTED_BOARD_ERROR = 14,
    BOARD_NOT_CREATED_ERROR = 15,
    ANOTHER_BOARD_IS_CREATED_ERROR = 16,
    GENERAL_ERROR = 17,
    SYNC_TIMEOUT_ERROR = 18,
    JSON_NOT_FOUND_ERROR = 19,
    NO_SUCH_DATA_IN_JSON_ERROR = 20,
    CLASSIFIER_IS_NOT_PREPARED_ERROR = 21,
    ANOTHER_CLASSIFIER_IS_PREPARED_ERROR = 22,
    UNSUPPORTED_CLASSIFIER_AND_METRIC_COMBINATION_ERROR = 23,
}

export enum BrainFlowMetrics {
    MINDFULNESS = 0,
    RESTFULNESS = 1,
    USER_DEFINED = 2,
}

export enum BrainFlowClassifiers {
    DEFAULT_CLASSIFIER = 0,
    USER_DEFINED = 1,
    ONNX_CLASSIFIER = 2,
}

export interface IBrainFlowInputParams {
    serialPort: string;
    macAddress: string;
    ipAddress: string;
    ipAddressAux: string;
    ipAddressAnc: string;
    ipPort: number;
    ipPortAux: number;
    ipPortAnc: number;
    ipProtocol: IpProtocolTypes;
    otherInfo: string;
    timeout: number;
    serialNumber: string;
    file: string;
    fileAux: string;
    fileAnc: string;
    masterBoard: BoardIds;
}

export interface IBrainFlowModelParams {
    metric: BrainFlowMetrics;
    classifier: BrainFlowClassifiers;
    file: string;
    otherInfo: string;
    outputName: string;
    maxArraySize: number;
}
