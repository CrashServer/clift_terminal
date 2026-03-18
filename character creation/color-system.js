/**
 * Vibrant Color System for Character Creator
 * Highly saturated colors inspired by 16-bit era games
 * Each color has 4 shades with maximum visual impact
 */

class ColorSystem {
    constructor() {
        this.DITHER = {
            LIGHT: [' ', '·', ':', '∴'],
            MEDIUM: ['░', '▒', '▓', '█'],
            STIPPLE: ['.', ':', ';', '%'],
            CROSSHATCH: ['╱', '╳', '▓', '█']
        };

        // VIBRANT colors - high saturation, strong contrast
        this.htmlColors = {
            // HAIR - Bold, saturated colors
            hair: {
                black: {
                    highlight: '#5050ff',  // Blue-ish highlight
                    base: '#202040',
                    shadow: '#101028',
                    deep: '#080818'
                },
                darkBrown: {
                    highlight: '#c07040',
                    base: '#803010',
                    shadow: '#501808',
                    deep: '#280800'
                },
                brown: {
                    highlight: '#e8a050',
                    base: '#b06020',
                    shadow: '#783008',
                    deep: '#401800'
                },
                lightBrown: {
                    highlight: '#ffd080',
                    base: '#d09030',
                    shadow: '#a06010',
                    deep: '#684000'
                },
                blonde: {
                    highlight: '#fffff0',
                    base: '#ffe030',
                    shadow: '#d0a000',
                    deep: '#a07800'
                },
                platinum: {
                    highlight: '#ffffff',
                    base: '#e8e8ff',
                    shadow: '#b0b0e0',
                    deep: '#8080b0'
                },
                red: {
                    highlight: '#ff6060',
                    base: '#e01010',
                    shadow: '#a00000',
                    deep: '#600000'
                },
                auburn: {
                    highlight: '#ff8040',
                    base: '#c04010',
                    shadow: '#802000',
                    deep: '#501000'
                },
                ginger: {
                    highlight: '#ffb020',
                    base: '#f07000',
                    shadow: '#b04800',
                    deep: '#702800'
                },
                gray: {
                    highlight: '#e8e8f0',
                    base: '#a0a0b0',
                    shadow: '#606070',
                    deep: '#383840'
                },
                white: {
                    highlight: '#ffffff',
                    base: '#f0f0ff',
                    shadow: '#d0d0e8',
                    deep: '#a0a0c0'
                },
                blue: {
                    highlight: '#00ffff',
                    base: '#00a0e0',
                    shadow: '#0060a0',
                    deep: '#003060'
                },
                purple: {
                    highlight: '#ff80ff',
                    base: '#c040c0',
                    shadow: '#802080',
                    deep: '#400840'
                },
                pink: {
                    highlight: '#ffb0d0',
                    base: '#ff5090',
                    shadow: '#c02060',
                    deep: '#801040'
                },
                green: {
                    highlight: '#80ff60',
                    base: '#40c020',
                    shadow: '#208010',
                    deep: '#104008'
                }
            },

            // EYES - Vivid, jewel-like colors
            eyes: {
                brown: {
                    highlight: '#ffa040',
                    base: '#c06020',
                    shadow: '#803010',
                    deep: '#401808',
                    iris: '#a04010',
                    pupil: '#000000'
                },
                darkBrown: {
                    highlight: '#a06030',
                    base: '#603010',
                    shadow: '#401808',
                    deep: '#200800',
                    iris: '#502008',
                    pupil: '#000000'
                },
                hazel: {
                    highlight: '#e0c040',
                    base: '#a08020',
                    shadow: '#706010',
                    deep: '#484000',
                    iris: '#908018',
                    pupil: '#000000'
                },
                green: {
                    highlight: '#40ff80',
                    base: '#20c040',
                    shadow: '#108028',
                    deep: '#084018',
                    iris: '#18a030',
                    pupil: '#001008'
                },
                blue: {
                    highlight: '#40c0ff',
                    base: '#2090e0',
                    shadow: '#1060a0',
                    deep: '#083060',
                    iris: '#1878c0',
                    pupil: '#000820'
                },
                gray: {
                    highlight: '#c0d0e0',
                    base: '#8090a8',
                    shadow: '#506070',
                    deep: '#303840',
                    iris: '#607080',
                    pupil: '#101820'
                },
                amber: {
                    highlight: '#ffe040',
                    base: '#f0a000',
                    shadow: '#c07000',
                    deep: '#804800',
                    iris: '#e09000',
                    pupil: '#201000'
                },
                violet: {
                    highlight: '#e080ff',
                    base: '#a040d0',
                    shadow: '#602080',
                    deep: '#301040',
                    iris: '#8030b0',
                    pupil: '#100820'
                },
                red: {
                    highlight: '#ff6060',
                    base: '#e02020',
                    shadow: '#a01010',
                    deep: '#600808',
                    iris: '#c01818',
                    pupil: '#200000'
                },
                gold: {
                    highlight: '#ffff60',
                    base: '#e0c020',
                    shadow: '#a08010',
                    deep: '#605000',
                    iris: '#c0a018',
                    pupil: '#201800'
                }
            },

            // SKIN - Warm, natural tones with strong shading
            skin: {
                pale: {
                    highlight: '#fff8f0',
                    base: '#ffd8c8',
                    shadow: '#d0a090',
                    deep: '#a07060',
                    blush: '#ff9090'
                },
                fair: {
                    highlight: '#fff0e0',
                    base: '#ffc8a0',
                    shadow: '#c89070',
                    deep: '#906048',
                    blush: '#ff8080'
                },
                light: {
                    highlight: '#ffe8d0',
                    base: '#e8b888',
                    shadow: '#b08058',
                    deep: '#785030',
                    blush: '#e08070'
                },
                medium: {
                    highlight: '#f0c8a0',
                    base: '#d09860',
                    shadow: '#a06830',
                    deep: '#684018',
                    blush: '#c07050'
                },
                tan: {
                    highlight: '#e0a878',
                    base: '#b87848',
                    shadow: '#885028',
                    deep: '#583018',
                    blush: '#a06040'
                },
                olive: {
                    highlight: '#d0b878',
                    base: '#a89050',
                    shadow: '#786830',
                    deep: '#504018',
                    blush: '#a08048'
                },
                brown: {
                    highlight: '#c08050',
                    base: '#905830',
                    shadow: '#603818',
                    deep: '#382008',
                    blush: '#884030'
                },
                darkBrown: {
                    highlight: '#a06840',
                    base: '#704020',
                    shadow: '#482810',
                    deep: '#281008',
                    blush: '#683020'
                },
                deep: {
                    highlight: '#806040',
                    base: '#503020',
                    shadow: '#302010',
                    deep: '#180808',
                    blush: '#503028'
                }
            },

            // ACCENT COLORS - For accessories
            accent: {
                gold: { highlight: '#ffff80', base: '#ffc020', shadow: '#c08000' },
                silver: { highlight: '#ffffff', base: '#c0c0d0', shadow: '#808898' },
                bronze: { highlight: '#e0a060', base: '#a06830', shadow: '#604020' },
                copper: { highlight: '#ffa060', base: '#c06020', shadow: '#803010' },
                ruby: { highlight: '#ff6080', base: '#e02040', shadow: '#901028' },
                sapphire: { highlight: '#60a0ff', base: '#2060e0', shadow: '#103090' },
                emerald: { highlight: '#60ff80', base: '#20c040', shadow: '#108028' },
                amethyst: { highlight: '#c080ff', base: '#8040c0', shadow: '#402080' },
                scar: { highlight: '#ffa0a0', base: '#e06060', shadow: '#a03030' },
                tattoo: { highlight: '#6080a0', base: '#304060', shadow: '#182030' },
                lips: { highlight: '#ff8090', base: '#e05060', shadow: '#a03040' }
            }
        };

        this.colors = {
            hair: {},
            eyes: {},
            skin: {},
            reset: '\x1b[0m'
        };
    }

    getShade(colorType, colorName, shade = 'base') {
        const colorSet = this.htmlColors[colorType]?.[colorName];
        if (!colorSet) return '#ff00ff'; // Magenta for missing colors
        return colorSet[shade] || colorSet.base;
    }

    getHTMLColor(colorType, colorName) {
        return this.getShade(colorType, colorName, 'base');
    }

    getColorRamp(colorType, colorName) {
        return this.htmlColors[colorType]?.[colorName] || null;
    }

    getDitherChar(intensity, patternType = 'MEDIUM') {
        const pattern = this.DITHER[patternType] || this.DITHER.MEDIUM;
        const index = Math.floor(intensity * (pattern.length - 1));
        return pattern[Math.min(index, pattern.length - 1)];
    }

    getShadeForPosition(normalizedPos, invert = false) {
        const pos = invert ? 1 - normalizedPos : normalizedPos;
        if (pos < 0.25) return 'highlight';
        if (pos < 0.5) return 'base';
        if (pos < 0.75) return 'shadow';
        return 'deep';
    }

    createDitherGradient(length, colorType, colorName, startShade, endShade) {
        const colors = this.htmlColors[colorType]?.[colorName];
        if (!colors) return ' '.repeat(length);

        const shadeOrder = ['highlight', 'base', 'shadow', 'deep'];
        const startIdx = shadeOrder.indexOf(startShade);
        const endIdx = shadeOrder.indexOf(endShade);

        let result = '';
        for (let i = 0; i < length; i++) {
            const t = i / (length - 1);
            const shadeIdx = Math.round(startIdx + (endIdx - startIdx) * t);
            const ditherIntensity = (shadeIdx + 1) / shadeOrder.length;
            result += this.getDitherChar(ditherIntensity);
        }
        return result;
    }

    applyColorToGrid(grid, colorType, colorName) {
        return {
            colorType,
            colorName,
            htmlColor: this.getHTMLColor(colorType, colorName)
        };
    }

    colorize(text, colorType, colorName) {
        return text;
    }
}
