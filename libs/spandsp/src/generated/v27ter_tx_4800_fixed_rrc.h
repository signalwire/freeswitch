#define TX_PULSESHAPER_4800_GAIN        0.875534f
#define TX_PULSESHAPER_4800_COEFF_SETS  5
static const int16_t tx_pulseshaper_4800[TX_PULSESHAPER_4800_COEFF_SETS][9] =
{
    {
              58,     /* Filter 0 */
             434,
            -155,
           -3327,
           21702,
           11548,
            -978,
            -560,
             141
    },
    {
            -164,     /* Filter 1 */
             439,
             657,
           -4647,
           29721,
            2524,
             770,
            -386,
            -176
    },
    {
            -291,     /* Filter 2 */
              87,
            1223,
           -3058,
           32767,
           -3058,
            1223,
              87,
            -291
    },
    {
            -176,     /* Filter 3 */
            -386,
             770,
            2524,
           29721,
           -4647,
             657,
             439,
            -164
    },
    {
             141,     /* Filter 4 */
            -560,
            -978,
           11548,
           21702,
           -3327,
            -155,
             434,
              58
    }
};
