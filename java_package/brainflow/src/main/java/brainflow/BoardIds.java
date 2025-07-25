package brainflow;

import java.util.HashMap;
import java.util.Map;

/**
 * enum to store all supported boards
 */
public enum BoardIds
{
    NO_BOARD (-100),
    PLAYBACK_FILE_BOARD (-3),
    STREAMING_BOARD (-2),
    SYNTHETIC_BOARD (-1),
    CYTON_BOARD (0),
    GANGLION_BOARD (1),
    CYTON_DAISY_BOARD (2),
    GALEA_BOARD (3),
    GANGLION_WIFI_BOARD (4),
    CYTON_WIFI_BOARD (5),
    CYTON_DAISY_WIFI_BOARD (6),
    BRAINBIT_BOARD (7),
    UNICORN_BOARD (8),
    CALLIBRI_EEG_BOARD (9),
    CALLIBRI_EMG_BOARD (10),
    CALLIBRI_ECG_BOARD (11),
    NOTION_1_BOARD (13),
    NOTION_2_BOARD (14),
    GFORCE_PRO_BOARD (16),
    FREEEEG32_BOARD (17),
    BRAINBIT_BLED_BOARD (18),
    GFORCE_DUAL_BOARD (19),
    GALEA_SERIAL_BOARD (20),
    MUSE_S_BLED_BOARD (21),
    MUSE_2_BLED_BOARD (22),
    CROWN_BOARD (23),
    ANT_NEURO_EE_410_BOARD (24),
    ANT_NEURO_EE_411_BOARD (25),
    ANT_NEURO_EE_430_BOARD (26),
    ANT_NEURO_EE_211_BOARD (27),
    ANT_NEURO_EE_212_BOARD (28),
    ANT_NEURO_EE_213_BOARD (29),
    ANT_NEURO_EE_214_BOARD (30),
    ANT_NEURO_EE_215_BOARD (31),
    ANT_NEURO_EE_221_BOARD (32),
    ANT_NEURO_EE_222_BOARD (33),
    ANT_NEURO_EE_223_BOARD (34),
    ANT_NEURO_EE_224_BOARD (35),
    ANT_NEURO_EE_225_BOARD (36),
    ENOPHONE_BOARD (37),
    MUSE_2_BOARD (38),
    MUSE_S_BOARD (39),
    BRAINALIVE_BOARD (40),
    MUSE_2016_BOARD (41),
    MUSE_2016_BLED_BOARD (42),
    EXPLORE_4_CHAN_BOARD (44),
    EXPLORE_8_CHAN_BOARD (45),
    GANGLION_NATIVE_BOARD (46),
    EMOTIBIT_BOARD (47),
    GALEA_BOARD_V4 (48),
    GALEA_SERIAL_BOARD_V4 (49),
    NTL_WIFI_BOARD (50),
    ANT_NEURO_EE_511_BOARD (51),
    FREEEEG128_BOARD (52),
    AAVAA_V3_BOARD(53),
    EXPLORE_PLUS_8_CHAN_BOARD(54),
    EXPLORE_PLUS_32_CHAN_BOARD(55),
    PIEEG_BOARD(56),
    NEUROPAWN_KNIGHT_BOARD(57),
    SYNCHRONI_TRIO_3_CHANNELS_BOARD(58),
    SYNCHRONI_OCTO_8_CHANNELS_BOARD(59),
    OB5000_8_CHANNELS_BOARD(60),
    SYNCHRONI_PENTO_8_CHANNELS_BOARD(61),
    SYNCHRONI_UNO_1_CHANNELS_BOARD(62),
    OB3000_24_CHANNELS_BOARD(63),
    BIOLISTENER_BOARD(64),
    CERELOG_X8_BOARD(65);

    private final int board_id;
    private static final Map<Integer, BoardIds> bi_map = new HashMap<Integer, BoardIds> ();

    public int get_code ()
    {
        return board_id;
    }

    public static String string_from_code (final int code)
    {
        return from_code (code).name ();
    }

    public static BoardIds from_code (final int code)
    {
        final BoardIds element = bi_map.get (code);
        return element;
    }

    BoardIds (final int code)
    {
        board_id = code;
    }

    static
    {
        for (final BoardIds bi : BoardIds.values ())
        {
            bi_map.put (bi.get_code (), bi);
        }
    }
}
