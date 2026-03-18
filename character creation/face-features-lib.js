/**
 * Comprehensive Face Feature Library
 * Each feature is defined as a drawing function on the grid
 */

const FaceFeatures = {

    /**
     * FACE SHAPES - Define the outer contour
     */
    faceShapes: {
        oval: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;

            // Draw oval outline using curves
            // Left side
            system.drawCurve(grid,
                FACE_LEFT - bodyMod, HAIRLINE,
                FACE_LEFT - bodyMod - 1, (HAIRLINE + CHIN) / 2,
                FACE_LEFT - bodyMod, CHIN,
                '('
            );

            // Right side
            system.drawCurve(grid,
                FACE_RIGHT + bodyMod, HAIRLINE,
                FACE_RIGHT + bodyMod + 1, (HAIRLINE + CHIN) / 2,
                FACE_RIGHT + bodyMod, CHIN,
                ')'
            );

            // Top curve
            for (let x = FACE_LEFT - bodyMod; x <= FACE_RIGHT + bodyMod; x++) {
                const y = HAIRLINE + Math.round(2 * Math.sin((x - CENTER) / 10));
                system.setChar(grid, x, y, '-');
            }

            // Chin curve
            for (let x = FACE_LEFT - bodyMod + 2; x <= FACE_RIGHT + bodyMod - 2; x++) {
                const y = CHIN - Math.round(Math.sin((x - CENTER) / 5));
                system.setChar(grid, x, y, '-');
            }
        },

        round: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;
            const radius = (CHIN - HAIRLINE) / 2;
            const centerY = HAIRLINE + radius;

            // Draw rounder face
            for (let angle = 0; angle < 360; angle += 5) {
                const rad = angle * Math.PI / 180;
                const x = Math.round(CENTER + (radius + 4 + bodyMod) * Math.cos(rad));
                const y = Math.round(centerY + radius * Math.sin(rad));

                if (y >= HAIRLINE && y <= CHIN) {
                    const char = (x < CENTER) ? '(' : ')';
                    system.setChar(grid, x, y, char);
                }
            }
        },

        square: (grid, system, props) => {
            const { HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;

            // Straight sides
            for (let y = HAIRLINE; y <= CHIN; y++) {
                system.setChar(grid, FACE_LEFT - bodyMod, y, '|');
                system.setChar(grid, FACE_RIGHT + bodyMod, y, '|');
            }

            // Top and bottom
            for (let x = FACE_LEFT - bodyMod; x <= FACE_RIGHT + bodyMod; x++) {
                system.setChar(grid, x, HAIRLINE, '_');
                system.setChar(grid, x, CHIN, '_');
            }
        },

        angular: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT, CHEEKBONE } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;
            const cheekbone = CHEEKBONE || HAIRLINE + 24;

            // Upper face - angles inward
            system.drawLine(grid, FACE_LEFT - bodyMod - 2, HAIRLINE, FACE_LEFT - bodyMod, cheekbone, '/');
            system.drawLine(grid, FACE_RIGHT + bodyMod + 2, HAIRLINE, FACE_RIGHT + bodyMod, cheekbone, '\\');

            // Lower face - angles to chin
            system.drawLine(grid, FACE_LEFT - bodyMod, cheekbone, CENTER - 2, CHIN, '\\');
            system.drawLine(grid, FACE_RIGHT + bodyMod, cheekbone, CENTER + 2, CHIN, '/');

            // Top line
            system.drawLine(grid, FACE_LEFT - bodyMod - 2, HAIRLINE, FACE_RIGHT + bodyMod + 2, HAIRLINE, '_');
        },

        long: (grid, system, props) => {
            const { CENTER, TOP_OF_HEAD, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;
            const extendedChin = CHIN + 5;

            // Elongated oval
            system.drawCurve(grid,
                FACE_LEFT - bodyMod, TOP_OF_HEAD + 5,
                FACE_LEFT - bodyMod - 1, (TOP_OF_HEAD + extendedChin) / 2,
                FACE_LEFT - bodyMod, extendedChin,
                '('
            );

            system.drawCurve(grid,
                FACE_RIGHT + bodyMod, TOP_OF_HEAD + 5,
                FACE_RIGHT + bodyMod + 1, (TOP_OF_HEAD + extendedChin) / 2,
                FACE_RIGHT + bodyMod, extendedChin,
                ')'
            );
        },

        heart: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;

            // Wider at top, narrow at bottom
            system.drawCurve(grid,
                FACE_LEFT - bodyMod - 2, HAIRLINE,
                FACE_LEFT - bodyMod, HAIRLINE + 8,
                CENTER - 1, CHIN,
                '\\'
            );

            system.drawCurve(grid,
                FACE_RIGHT + bodyMod + 2, HAIRLINE,
                FACE_RIGHT + bodyMod, HAIRLINE + 8,
                CENTER + 1, CHIN,
                '/'
            );

            // Widow's peak
            system.setChar(grid, CENTER, HAIRLINE - 1, 'V');
        },

        diamond: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT, CHEEKBONE } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;
            const cheekbone = CHEEKBONE || HAIRLINE + 24;

            // Narrow at top, wide at cheeks, narrow at chin
            system.drawLine(grid, CENTER - 3, HAIRLINE, FACE_LEFT - bodyMod - 1, cheekbone, '\\');
            system.drawLine(grid, CENTER + 3, HAIRLINE, FACE_RIGHT + bodyMod + 1, cheekbone, '/');
            system.drawLine(grid, FACE_LEFT - bodyMod - 1, cheekbone, CENTER - 2, CHIN, '/');
            system.drawLine(grid, FACE_RIGHT + bodyMod + 1, cheekbone, CENTER + 2, CHIN, '\\');
        },

        triangular: (grid, system, props) => {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;

            // Narrow at top, wide at bottom
            system.drawLine(grid, CENTER - 2, HAIRLINE, FACE_LEFT - bodyMod - 1, CHIN, '\\');
            system.drawLine(grid, CENTER + 2, HAIRLINE, FACE_RIGHT + bodyMod + 1, CHIN, '/');
            system.drawLine(grid, FACE_LEFT - bodyMod - 1, CHIN, FACE_RIGHT + bodyMod + 1, CHIN, '_');
        },

        oblong: (grid, system, props) => {
            const { CENTER, TOP_OF_HEAD, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bodyMod = props.bodyType || 0;
            const extendedTop = TOP_OF_HEAD + 3;
            const extendedChin = CHIN + 6;

            // Very elongated face
            system.drawCurve(grid,
                FACE_LEFT - bodyMod, extendedTop,
                FACE_LEFT - bodyMod - 1, (extendedTop + extendedChin) / 2,
                FACE_LEFT - bodyMod, extendedChin,
                '('
            );

            system.drawCurve(grid,
                FACE_RIGHT + bodyMod, extendedTop,
                FACE_RIGHT + bodyMod + 1, (extendedTop + extendedChin) / 2,
                FACE_RIGHT + bodyMod, extendedChin,
                ')'
            );
        }
    },

    /**
     * EYES - Most important feature
     */
    eyes: {
        normal: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Left eye
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '(');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, 'o');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, ')');

                // Right eye
                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, 'o');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');

                // Eyebrows
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '-');
            }
        },

        wide: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE, EYE_LEFT_CENTER + 3, EYE_LINE, '=');
                system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE, EYE_RIGHT_CENTER + 3, EYE_LINE, '=');
            } else {
                // Larger eyes
                system.fillCircle(grid, EYE_LEFT_CENTER, EYE_LINE, 2, 'O');
                system.fillCircle(grid, EYE_RIGHT_CENTER, EYE_LINE, 2, 'O');

                // Pupils
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '●');

                // Wide eyebrows
                system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 2, EYE_LEFT_CENTER + 3, EYE_LINE - 2, '‾');
                system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE - 2, EYE_RIGHT_CENTER + 3, EYE_LINE - 2, '‾');
            }
        },

        narrow: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            // Always narrow/squinting
            system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE, '-');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '•');
            system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE, '-');

            system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE, '-');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '•');
            system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE, '-');

            // Narrow brows
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '\'');
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '\'');
        },

        almond: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER, EYE_LINE + 1, EYE_LEFT_CENTER + 2, EYE_LINE, '~');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER, EYE_LINE + 1, EYE_RIGHT_CENTER + 2, EYE_LINE, '~');
            } else {
                // Almond shape
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '/');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◠');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, '\\');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '/');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◠');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, '\\');

                // Curved brows
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER, EYE_LINE - 3, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '~');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, EYE_RIGHT_CENTER, EYE_LINE - 3, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '~');
            }
        },

        hooded: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            // Heavy upper lids
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '_');
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '_');

            if (!isBlinking) {
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◕');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◕');
            }

            // Heavy brows
            system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 2, EYE_LEFT_CENTER + 3, EYE_LINE - 2, '▀');
            system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE - 2, EYE_RIGHT_CENTER + 3, EYE_LINE - 2, '▀');
        },

        round: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.fillEllipse(grid, EYE_LEFT_CENTER, EYE_LINE, 2, 0, '-');
                system.fillEllipse(grid, EYE_RIGHT_CENTER, EYE_LINE, 2, 0, '-');
            } else {
                // Very round eyes
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 1, '◜');
                system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, '◝');

                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 1, '◜');
                system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, '◝');

                // Round brows
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 3, EYE_LEFT_CENTER, EYE_LINE - 4, EYE_LEFT_CENTER + 2, EYE_LINE - 3, '-');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 3, EYE_RIGHT_CENTER, EYE_LINE - 4, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '-');
            }
        },

        tired: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            // Droopy eyes
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '˘');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, ')');

            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '˘');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');

            // Bags under eyes
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, '‾');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, '‾');

            // Droopy brows
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, '\\');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 2, '_');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '/');

            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, '\\');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 2, '_');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '/');
        },

        deepset: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            // Heavy upper lids and shadow
            system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 1, EYE_LEFT_CENTER + 3, EYE_LINE - 1, '▄');
            system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE - 1, EYE_RIGHT_CENTER + 3, EYE_LINE - 1, '▄');

            if (!isBlinking) {
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◉');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◉');
            }

            // Prominent brows
            system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 2, EYE_LEFT_CENTER + 3, EYE_LINE - 2, '▀');
            system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE - 2, EYE_RIGHT_CENTER + 3, EYE_LINE - 2, '▀');
        },

        protruding: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.fillEllipse(grid, EYE_LEFT_CENTER, EYE_LINE, 2, 0, '-');
                system.fillEllipse(grid, EYE_RIGHT_CENTER, EYE_LINE, 2, 0, '-');
            } else {
                // Large protruding eyes
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 1, '◠');
                system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◉');
                system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, '◡');

                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 1, '◠');
                system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◉');
                system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, '◡');
            }

            // Thin brows
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 3, EYE_LEFT_CENTER + 2, EYE_LINE - 3, '\'');
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 3, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '\'');
        },

        monolid: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Single eyelid
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '⌐');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, '¬');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '⌐');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, '¬');

                // Straight brows
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '▔');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '▔');
            }
        },

        upturned: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE + 1, EYE_LEFT_CENTER, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '~');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, EYE_RIGHT_CENTER, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '~');
            } else {
                // Upturned cat-like eyes
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE + 1, '\\');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◕');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '/');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, '\\');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◕');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '/');

                // Arched brows
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER, EYE_LINE - 4, EYE_LEFT_CENTER + 2, EYE_LINE - 3, '~');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, EYE_RIGHT_CENTER, EYE_LINE - 4, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '~');
            }
        },

        downturned: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, EYE_LEFT_CENTER, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '~');
                system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, EYE_RIGHT_CENTER, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '~');
            } else {
                // Downturned sad eyes
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, '/');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◔');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '\\');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, '/');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◔');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '\\');

                // Concerned brows
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, '/');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 3, '‾');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '\\');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, '/');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 3, '‾');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '\\');
            }
        }
    },

    /**
     * NOSES
     */
    noses: {
        small: (grid, system) => {
            const { CENTER, NOSE_BASE } = system.PROPORTIONS;
            system.setChar(grid, CENTER, NOSE_BASE - 1, '|');
            system.setChar(grid, CENTER, NOSE_BASE, 'v');
        },

        medium: (grid, system) => {
            const { CENTER, NOSE_BASE, EYE_LINE } = system.PROPORTIONS;
            const bridgeStart = EYE_LINE + 2;

            // Bridge
            for (let y = bridgeStart; y < NOSE_BASE - 1; y++) {
                system.setChar(grid, CENTER, y, '|');
            }

            // Tip and nostrils
            system.setChar(grid, CENTER - 1, NOSE_BASE - 1, '/');
            system.setChar(grid, CENTER, NOSE_BASE - 1, '|');
            system.setChar(grid, CENTER + 1, NOSE_BASE - 1, '\\');
            system.setChar(grid, CENTER - 1, NOSE_BASE, '(');
            system.setChar(grid, CENTER + 1, NOSE_BASE, ')');
        },

        large: (grid, system) => {
            const { CENTER, NOSE_BASE, EYE_LINE } = system.PROPORTIONS;
            const bridgeStart = EYE_LINE + 2;

            // Wide bridge
            for (let y = bridgeStart; y < NOSE_BASE - 2; y++) {
                system.setChar(grid, CENTER - 1, y, '/');
                system.setChar(grid, CENTER + 1, y, '\\');
            }

            // Large tip
            system.setChar(grid, CENTER - 2, NOSE_BASE - 1, '/');
            system.setChar(grid, CENTER, NOSE_BASE - 1, '▼');
            system.setChar(grid, CENTER + 2, NOSE_BASE - 1, '\\');

            // Wide nostrils
            system.setChar(grid, CENTER - 2, NOSE_BASE, '(');
            system.setChar(grid, CENTER - 1, NOSE_BASE, '°');
            system.setChar(grid, CENTER + 1, NOSE_BASE, '°');
            system.setChar(grid, CENTER + 2, NOSE_BASE, ')');
        },

        hooked: (grid, system) => {
            const { CENTER, NOSE_BASE, EYE_LINE } = system.PROPORTIONS;
            const bridgeStart = EYE_LINE + 2;

            // Curved bridge
            system.drawCurve(grid,
                CENTER, bridgeStart,
                CENTER + 2, bridgeStart + 3,
                CENTER, NOSE_BASE,
                '/'
            );

            system.setChar(grid, CENTER - 1, NOSE_BASE, '(');
            system.setChar(grid, CENTER + 1, NOSE_BASE, ')');
        },

        button: (grid, system) => {
            const { CENTER, NOSE_BASE } = system.PROPORTIONS;

            // Small round button
            system.setChar(grid, CENTER, NOSE_BASE - 1, 'o');
            system.setChar(grid, CENTER - 1, NOSE_BASE, '˘');
            system.setChar(grid, CENTER + 1, NOSE_BASE, '˘');
        },

        flat: (grid, system) => {
            const { CENTER, NOSE_BASE } = system.PROPORTIONS;

            // Minimal nose
            system.setChar(grid, CENTER - 1, NOSE_BASE, '=');
            system.setChar(grid, CENTER, NOSE_BASE, '=');
            system.setChar(grid, CENTER + 1, NOSE_BASE, '=');
        },

        pointed: (grid, system) => {
            const { CENTER, NOSE_BASE, EYE_LINE } = system.PROPORTIONS;
            const bridgeStart = EYE_LINE + 3;

            // Sharp point
            system.drawLine(grid, CENTER, bridgeStart, CENTER, NOSE_BASE - 2, '|');
            system.setChar(grid, CENTER, NOSE_BASE - 1, 'V');
            system.setChar(grid, CENTER - 1, NOSE_BASE, '^');
            system.setChar(grid, CENTER + 1, NOSE_BASE, '^');
        }
    },

    /**
     * MOUTHS
     */
    mouths: {
        small: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            const shapes = [
                [['(', 'o', ')']],
                [['(', '.', ')']],
                [['(', 'o', ')']]
            ];

            const shape = shapes[frame % shapes.length];
            shape[0].forEach((char, i) => {
                system.setChar(grid, CENTER - 1 + i, MOUTH_CENTER, char);
            });
        },

        normal: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            const shapes = [
                [' ', '(', '-', '-', '-', ')', ' '],
                [' ', '(', '‾', '‾', '‾', ')', ' '],
                [' ', '(', '_', '_', '_', ')', ' ']
            ];

            const shape = shapes[frame % shapes.length];
            shape.forEach((char, i) => {
                system.setChar(grid, CENTER - 3 + i, MOUTH_CENTER, char);
            });
        },

        wide: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;

            // Wide smile
            system.setChar(grid, CENTER - 5, MOUTH_CENTER, '(');
            for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                system.setChar(grid, x, MOUTH_CENTER, frame % 2 === 0 ? '=' : '-');
            }
            system.setChar(grid, CENTER + 5, MOUTH_CENTER, ')');
        },

        thin: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;

            // Thin line
            for (let x = CENTER - 3; x <= CENTER + 3; x++) {
                system.setChar(grid, x, MOUTH_CENTER, frame % 2 === 0 ? '_' : '‾');
            }
        },

        full: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;

            // Full lips - two lines
            system.drawCurve(grid, CENTER - 3, MOUTH_CENTER - 1, CENTER, MOUTH_CENTER - 2, CENTER + 3, MOUTH_CENTER - 1, '▄');
            system.drawCurve(grid, CENTER - 3, MOUTH_CENTER + 1, CENTER, MOUTH_CENTER + 2, CENTER + 3, MOUTH_CENTER + 1, '▀');

            for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                system.setChar(grid, x, MOUTH_CENTER, frame % 2 === 0 ? '═' : '━');
            }
        },

        smirk: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;

            // Asymmetric smirk
            system.setChar(grid, CENTER - 2, MOUTH_CENTER, '(');
            system.setChar(grid, CENTER - 1, MOUTH_CENTER, '-');
            system.setChar(grid, CENTER, MOUTH_CENTER, '-');
            system.setChar(grid, CENTER + 1, MOUTH_CENTER, '-');
            system.setChar(grid, CENTER + 2, MOUTH_CENTER, ')');
            system.setChar(grid, CENTER + 3, MOUTH_CENTER, frame % 2 === 0 ? '/' : '\\');
        },

        // Expressive mouths
        smiling: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            // Curved upward smile
            system.setChar(grid, CENTER - 4, MOUTH_CENTER, '\\');
            system.setChar(grid, CENTER - 3, MOUTH_CENTER + 1, '\\');
            system.setChar(grid, CENTER - 2, MOUTH_CENTER + 1, '_');
            system.setChar(grid, CENTER, MOUTH_CENTER + 1, '_');
            system.setChar(grid, CENTER + 2, MOUTH_CENTER + 1, '_');
            system.setChar(grid, CENTER + 3, MOUTH_CENTER + 1, '/');
            system.setChar(grid, CENTER + 4, MOUTH_CENTER, '/');
        },

        frowning: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            // Curved downward frown
            system.setChar(grid, CENTER - 4, MOUTH_CENTER + 1, '/');
            system.setChar(grid, CENTER - 3, MOUTH_CENTER, '/');
            system.setChar(grid, CENTER - 2, MOUTH_CENTER, '‾');
            system.setChar(grid, CENTER, MOUTH_CENTER, '‾');
            system.setChar(grid, CENTER + 2, MOUTH_CENTER, '‾');
            system.setChar(grid, CENTER + 3, MOUTH_CENTER, '\\');
            system.setChar(grid, CENTER + 4, MOUTH_CENTER + 1, '\\');
        },

        laughing: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            // Open mouth showing teeth
            system.setChar(grid, CENTER - 4, MOUTH_CENTER - 1, '/');
            system.setChar(grid, CENTER - 3, MOUTH_CENTER - 1, '‾');
            for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                system.setChar(grid, x, MOUTH_CENTER - 1, '‾');
            }
            system.setChar(grid, CENTER + 3, MOUTH_CENTER - 1, '‾');
            system.setChar(grid, CENTER + 4, MOUTH_CENTER - 1, '\\');

            // Teeth
            for (let x = CENTER - 3; x <= CENTER + 3; x++) {
                system.setChar(grid, x, MOUTH_CENTER, frame % 2 === 0 ? '=' : '≡');
            }

            // Bottom
            for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                system.setChar(grid, x, MOUTH_CENTER + 1, '_');
            }
        },

        neutral: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            // Straight line
            for (let x = CENTER - 3; x <= CENTER + 3; x++) {
                system.setChar(grid, x, MOUTH_CENTER, '─');
            }
        },

        talking: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            const frames = [
                // Frame 0
                () => {
                    system.setChar(grid, CENTER - 2, MOUTH_CENTER, '(');
                    system.setChar(grid, CENTER, MOUTH_CENTER, 'o');
                    system.setChar(grid, CENTER + 2, MOUTH_CENTER, ')');
                },
                // Frame 1
                () => {
                    system.setChar(grid, CENTER - 2, MOUTH_CENTER, '(');
                    system.setChar(grid, CENTER, MOUTH_CENTER, '◦');
                    system.setChar(grid, CENTER + 2, MOUTH_CENTER, ')');
                },
                // Frame 2
                () => {
                    for (let x = CENTER - 2; x <= CENTER + 2; x++) {
                        system.setChar(grid, x, MOUTH_CENTER, '─');
                    }
                }
            ];
            frames[frame % frames.length]();
        },

        kissing: (grid, system, frame) => {
            const { CENTER, MOUTH_CENTER } = system.PROPORTIONS;
            // Pursed lips
            system.setChar(grid, CENTER - 1, MOUTH_CENTER, '(');
            system.setChar(grid, CENTER, MOUTH_CENTER, '3');
            system.setChar(grid, CENTER + 1, MOUTH_CENTER, ')');
        }
    },

    /**
     * EXPRESSIVE EYES - For different emotions
     */
    expressiveEyes: {
        happy: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes for blinking
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Curved/squinting eyes
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '╱');
                system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE, '‾');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '^');
                system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE, '‾');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, '╲');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '╱');
                system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE, '‾');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '^');
                system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE, '‾');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, '╲');
            }

            // Raised brows (always shown)
            system.drawCurve(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 3, EYE_LEFT_CENTER, EYE_LINE - 4, EYE_LEFT_CENTER + 2, EYE_LINE - 3, '‾');
            system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 3, EYE_RIGHT_CENTER, EYE_LINE - 4, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '‾');
        },

        sad: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes for blinking
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Downturned eyes
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, '/');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◔');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '\\');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, '/');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◔');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '\\');

                // Optional teary eyes
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, '˙');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, '˙');
            }

            // Sad brows (always shown)
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, '/');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 3, '‾');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '\\');

            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, '/');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 3, '‾');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 2, '\\');
        },

        angry: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes for blinking
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Angled eyes
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '<');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, '>');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '<');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '●');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, '>');
            }

            // Furrowed brows (always shown)
            system.setChar(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 2, '\\');
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 3, '\\');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 2, '▀');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '▀');

            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 2, '▀');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 2, '▀');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '/');
            system.setChar(grid, EYE_RIGHT_CENTER + 3, EYE_LINE - 2, '/');
        },

        surprised: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes for blinking
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // Wide eyes
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE - 1, '◠');
                system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, '◉');
                system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, '◡');

                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE - 1, '◠');
                system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE, '(');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, '◉');
                system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE, ')');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, '◡');
            }

            // Raised brows (always shown)
            system.drawLine(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 4, EYE_LEFT_CENTER + 3, EYE_LINE - 4, '‾');
            system.drawLine(grid, EYE_RIGHT_CENTER - 3, EYE_LINE - 4, EYE_RIGHT_CENTER + 3, EYE_LINE - 4, '‾');
        },

        skeptical: (grid, system, isBlinking) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            if (isBlinking) {
                // Closed eyes for blinking
                system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE, EYE_LEFT_CENTER + 2, EYE_LINE, '-');
                system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, EYE_RIGHT_CENTER + 2, EYE_LINE, '-');
            } else {
                // One raised eyebrow
                system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '(');
                system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE, 'o');
                system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, ')');

                system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
                system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE, 'o');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');
            }

            // Normal left brow
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 2, EYE_LEFT_CENTER + 2, EYE_LINE - 2, '-');

            // Raised right brow
            system.drawCurve(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 3, EYE_RIGHT_CENTER, EYE_LINE - 4, EYE_RIGHT_CENTER + 2, EYE_LINE - 3, '‾');
        }
    },

    /**
     * ACCESSORIES - Glasses, earrings, hats, etc.
     */
    accessories: {
        // Glasses
        glassesRound: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Left lens
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, '◜');
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE + 1, '◝');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '◝');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, ')');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '◜');

            // Right lens
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, '◜');
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, '◝');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '◝');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '◜');

            // Bridge
            system.drawLine(grid, EYE_LEFT_CENTER + 3, EYE_LINE, EYE_RIGHT_CENTER - 3, EYE_LINE, '─');
        },

        glassesSquare: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Left lens
            for (let y = EYE_LINE - 1; y <= EYE_LINE + 1; y++) {
                system.setChar(grid, EYE_LEFT_CENTER - 2, y, '│');
                system.setChar(grid, EYE_LEFT_CENTER + 2, y, '│');
            }
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '─');
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE + 1, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '─');

            // Right lens
            for (let y = EYE_LINE - 1; y <= EYE_LINE + 1; y++) {
                system.setChar(grid, EYE_RIGHT_CENTER - 2, y, '│');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, y, '│');
            }
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '─');
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '─');

            // Bridge
            system.drawLine(grid, EYE_LEFT_CENTER + 3, EYE_LINE, EYE_RIGHT_CENTER - 3, EYE_LINE, '─');
        },

        sunglasses: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Dark lenses
            for (let y = EYE_LINE - 1; y <= EYE_LINE + 1; y++) {
                for (let x = EYE_LEFT_CENTER - 2; x <= EYE_LEFT_CENTER + 2; x++) {
                    system.setChar(grid, x, y, '▓');
                }
                for (let x = EYE_RIGHT_CENTER - 2; x <= EYE_RIGHT_CENTER + 2; x++) {
                    system.setChar(grid, x, y, '▓');
                }
            }
            // Bridge
            system.drawLine(grid, EYE_LEFT_CENTER + 3, EYE_LINE, EYE_RIGHT_CENTER - 3, EYE_LINE, '═');
        },

        monocle: (grid, system) => {
            const { EYE_LINE, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Single lens on right eye
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, '◜');
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, '◝');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '◝');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '◜');
        },

        // Earrings
        earringStuds: (grid, system) => {
            const { EYE_LINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 4, '°');
            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 4, '°');
        },

        earringHoops: (grid, system) => {
            const { EYE_LINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 3, '◠');
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 4, '◡');
            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 3, '◠');
            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 4, '◡');
        },

        earringDangly: (grid, system) => {
            const { EYE_LINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 3, '◦');
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 4, '|');
            system.setChar(grid, FACE_LEFT + 1, EYE_LINE + 5, '◊');

            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 3, '◦');
            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 4, '|');
            system.setChar(grid, FACE_RIGHT - 1, EYE_LINE + 5, '◊');
        },

        // Hats
        topHat: (grid, system) => {
            const { CENTER, TOP_OF_HEAD, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const hatTop = TOP_OF_HEAD - 8;
            const hatBrim = TOP_OF_HEAD - 2;

            // Top of hat
            system.drawLine(grid, CENTER - 3, hatTop, CENTER + 3, hatTop, '▀');
            for (let y = hatTop + 1; y < hatBrim; y++) {
                system.setChar(grid, CENTER - 3, y, '║');
                system.setChar(grid, CENTER + 3, y, '║');
            }

            // Brim
            system.drawLine(grid, CENTER - 6, hatBrim, CENTER + 6, hatBrim, '▀');
        },

        cap: (grid, system) => {
            const { CENTER, TOP_OF_HEAD, FACE_RIGHT } = system.PROPORTIONS;
            const capTop = TOP_OF_HEAD - 3;

            // Cap dome
            for (let x = CENTER - 4; x <= CENTER + 3; x++) {
                system.setChar(grid, x, capTop, '▓');
                system.setChar(grid, x, capTop + 1, '▓');
            }

            // Bill
            for (let x = FACE_RIGHT; x <= FACE_RIGHT + 3; x++) {
                system.setChar(grid, x, capTop + 2, '▀');
            }
        },

        beanie: (grid, system) => {
            const { CENTER, TOP_OF_HEAD, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const beanieTop = TOP_OF_HEAD - 4;

            // Top pom-pom
            system.setChar(grid, CENTER, beanieTop, '●');

            // Beanie body
            for (let y = beanieTop + 1; y <= TOP_OF_HEAD; y++) {
                for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                    const char = (x + y) % 2 === 0 ? '▓' : '▒';
                    system.setChar(grid, x, y, char);
                }
            }
        },

        headband: (grid, system) => {
            const { HAIRLINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                system.setChar(grid, x, HAIRLINE, '▓');
            }
        },

        eyePatch: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER } = system.PROPORTIONS;
            // Patch over left eye
            for (let y = EYE_LINE - 1; y <= EYE_LINE + 1; y++) {
                for (let x = EYE_LEFT_CENTER - 2; x <= EYE_LEFT_CENTER + 2; x++) {
                    system.setChar(grid, x, y, '█');
                }
            }
        },

        faceMask: (grid, system) => {
            const { CENTER, NOSE_BASE, MOUTH_CENTER, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            // Mask covering nose and mouth
            for (let y = NOSE_BASE; y <= MOUTH_CENTER + 2; y++) {
                for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x++) {
                    system.setChar(grid, x, y, '░');
                }
            }
            // Ear straps
            system.setChar(grid, FACE_LEFT + 1, NOSE_BASE + 1, '─');
            system.setChar(grid, FACE_RIGHT - 1, NOSE_BASE + 1, '─');
        },

        necklace: (grid, system) => {
            const { CENTER, CHIN } = system.PROPORTIONS;
            const neckY = CHIN + 2;
            // Simple necklace
            for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                system.setChar(grid, x, neckY, '◦');
            }
            // Pendant
            system.setChar(grid, CENTER, neckY + 1, '◊');
        },

        // Additional glasses styles
        readingGlasses: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Half-moon reading glasses
            for (let y = EYE_LINE; y <= EYE_LINE + 1; y++) {
                system.setChar(grid, EYE_LEFT_CENTER - 2, y, '│');
                system.setChar(grid, EYE_LEFT_CENTER + 2, y, '│');
                system.setChar(grid, EYE_RIGHT_CENTER - 2, y, '│');
                system.setChar(grid, EYE_RIGHT_CENTER + 2, y, '│');
            }
            system.drawLine(grid, EYE_LEFT_CENTER - 2, EYE_LINE + 1, EYE_LEFT_CENTER + 2, EYE_LINE + 1, '─');
            system.drawLine(grid, EYE_RIGHT_CENTER - 2, EYE_LINE + 1, EYE_RIGHT_CENTER + 2, EYE_LINE + 1, '─');
            // Bridge
            system.drawLine(grid, EYE_LEFT_CENTER + 3, EYE_LINE, EYE_RIGHT_CENTER - 3, EYE_LINE, '─');
        },

        aviator: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Teardrop aviator shape
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE - 1, '◜');
            system.setChar(grid, EYE_LEFT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_LEFT_CENTER - 1, EYE_LINE + 1, '\\');
            system.setChar(grid, EYE_LEFT_CENTER, EYE_LINE + 1, 'v');
            system.setChar(grid, EYE_LEFT_CENTER + 1, EYE_LINE + 1, '/');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE, ')');
            system.setChar(grid, EYE_LEFT_CENTER + 2, EYE_LINE - 1, '◝');

            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE - 1, '◜');
            system.setChar(grid, EYE_RIGHT_CENTER - 2, EYE_LINE, '(');
            system.setChar(grid, EYE_RIGHT_CENTER - 1, EYE_LINE + 1, '\\');
            system.setChar(grid, EYE_RIGHT_CENTER, EYE_LINE + 1, 'v');
            system.setChar(grid, EYE_RIGHT_CENTER + 1, EYE_LINE + 1, '/');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE, ')');
            system.setChar(grid, EYE_RIGHT_CENTER + 2, EYE_LINE - 1, '◝');

            // Bridge
            system.drawLine(grid, EYE_LEFT_CENTER + 3, EYE_LINE, EYE_RIGHT_CENTER - 3, EYE_LINE, '─');
        },

        // More hats
        fedora: (grid, system) => {
            const { CENTER, TOP_OF_HEAD, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const hatTop = TOP_OF_HEAD - 5;
            const hatBrim = TOP_OF_HEAD - 1;

            // Crown
            for (let y = hatTop; y < hatBrim; y++) {
                for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                    system.setChar(grid, x, y, '▓');
                }
            }

            // Brim
            for (let x = CENTER - 7; x <= CENTER + 7; x++) {
                system.setChar(grid, x, hatBrim, '▀');
            }

            // Crease
            system.drawLine(grid, CENTER - 2, hatTop + 1, CENTER + 2, hatTop + 1, '‾');
        },

        bandana: (grid, system) => {
            const { CENTER, TOP_OF_HEAD, HAIRLINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            // Bandana tied at top/back
            for (let x = FACE_LEFT; x <= FACE_RIGHT; x++) {
                const char = (x + TOP_OF_HEAD) % 3 === 0 ? '▓' : x % 2 === 0 ? '▒' : '░';
                system.setChar(grid, x, HAIRLINE, char);
                system.setChar(grid, x, HAIRLINE + 1, char);
            }

            // Knot at back
            const knotX = FACE_RIGHT + 2;
            const knotY = TOP_OF_HEAD + 1;
            system.setChar(grid, knotX, knotY, '◉');
            system.setChar(grid, knotX - 1, knotY - 1, '╲');
            system.setChar(grid, knotX + 1, knotY - 1, '╱');
        },

        choker: (grid, system) => {
            const { CENTER, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const neckY = CHIN + 1;
            // Tight choker necklace
            for (let x = CENTER - 5; x <= CENTER + 5; x++) {
                system.setChar(grid, x, neckY, '═');
            }
            // Pendant
            system.setChar(grid, CENTER, neckY + 1, '♦');
        }
    },

    /**
     * HAIR OPTIONS - Advanced styling
     */
    hairOptions: {
        // Parting styles
        centerPart(grid, system, hairBounds) {
            const { CENTER, TOP_OF_HEAD, HAIRLINE } = system.PROPORTIONS;
            // Create center part line
            for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                if (grid[y] && grid[y][CENTER] && grid[y][CENTER] !== ' ') {
                    system.setChar(grid, CENTER, y, '│');
                }
            }
        },

        sidePart(grid, system, hairBounds) {
            const { CENTER, TOP_OF_HEAD, HAIRLINE } = system.PROPORTIONS;
            const partX = CENTER - 3;
            // Create side part line
            for (let y = TOP_OF_HEAD; y <= HAIRLINE; y++) {
                if (grid[y] && grid[y][partX] && grid[y][partX] !== ' ') {
                    system.setChar(grid, partX, y, '│');
                }
            }
        },

        // Bangs variations
        straightBangs(grid, system) {
            const { CENTER, HAIRLINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const bangsY = HAIRLINE + 2;
            for (let x = FACE_LEFT + 2; x <= FACE_RIGHT - 2; x++) {
                system.setChar(grid, x, bangsY, '║');
                system.setChar(grid, x, bangsY + 1, '║');
            }
        },

        sweptBangs(grid, system) {
            const { CENTER, HAIRLINE, FACE_LEFT, FACE_RIGHT, EYE_LINE } = system.PROPORTIONS;
            // Swept to the side
            for (let y = HAIRLINE + 2; y <= EYE_LINE - 1; y++) {
                const offset = Math.floor((y - HAIRLINE) / 2);
                const x = CENTER + offset;
                if (x <= FACE_RIGHT - 2) {
                    system.setChar(grid, x, y, '╱');
                    system.setChar(grid, x + 1, y, '╱');
                }
            }
        },

        curtainBangs(grid, system) {
            const { CENTER, HAIRLINE, EYE_LINE } = system.PROPORTIONS;
            // Bangs parted in center
            for (let y = HAIRLINE + 2; y <= EYE_LINE - 1; y++) {
                const offset = Math.floor((y - HAIRLINE) / 2);
                system.setChar(grid, CENTER - 2 - offset, y, '╲');
                system.setChar(grid, CENTER + 2 + offset, y, '╱');
            }
        },

        // Wind direction effects
        windLeft(grid, system, hairStyle, animState = {}) {
            const { FACE_LEFT, FACE_RIGHT, HAIRLINE, NOSE_BASE } = system.PROPORTIONS;
            const hairPhase = animState.hairPhase || 0;

            // Add wind-blown strands to the left with gentle sway
            for (let y = HAIRLINE; y <= NOSE_BASE; y++) {
                if (grid[y] && grid[y][FACE_RIGHT]) {
                    const windChars = ['/', '╱', '⁄'];
                    const char = windChars[y % windChars.length];
                    system.setChar(grid, FACE_RIGHT + 1, y, char);

                    // Use sine wave for gentle sway instead of random
                    const sway = Math.sin((hairPhase + y * 10) * Math.PI / 180);
                    if (sway > 0.3) {
                        system.setChar(grid, FACE_RIGHT + 2, y, char);
                    }
                }
            }
        },

        windRight(grid, system, hairStyle, animState = {}) {
            const { FACE_LEFT, FACE_RIGHT, HAIRLINE, NOSE_BASE } = system.PROPORTIONS;
            const hairPhase = animState.hairPhase || 0;

            // Add wind-blown strands to the right with gentle sway
            for (let y = HAIRLINE; y <= NOSE_BASE; y++) {
                if (grid[y] && grid[y][FACE_LEFT]) {
                    const windChars = ['\\', '╲', '⁄'];
                    const char = windChars[y % windChars.length];
                    system.setChar(grid, FACE_LEFT - 1, y, char);

                    // Use sine wave for gentle sway instead of random
                    const sway = Math.sin((hairPhase + y * 10) * Math.PI / 180);
                    if (sway > 0.3) {
                        system.setChar(grid, FACE_LEFT - 2, y, char);
                    }
                }
            }
        }
    },

    /**
     * SHADING & DEPTH - For 3D appearance
     */
    shading: {
        // Helper: Find face boundary on a given row
        findFaceBoundary(grid, y, center) {
            let leftEdge = null;
            let rightEdge = null;

            if (!grid[y]) return { leftEdge, rightEdge };

            // Scan left from center
            for (let x = center; x >= 0; x--) {
                const char = grid[y][x];
                if (char && char !== ' ' && char !== '·' && char !== '░' && char !== '▒' && char !== '▓') {
                    leftEdge = x;
                    break;
                }
            }

            // Scan right from center
            for (let x = center; x < grid[y].length; x++) {
                const char = grid[y][x];
                if (char && char !== ' ' && char !== '·' && char !== '░' && char !== '▒' && char !== '▓') {
                    rightEdge = x;
                    break;
                }
            }

            return { leftEdge, rightEdge };
        },

        // Face contour shading based on lighting - FOLLOWS ACTUAL FACE SHAPE
        faceContour(grid, system, lightDirection, intensity) {
            const { CENTER, HAIRLINE, CHIN, EYE_LINE, NOSE_BASE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const shadeChars = ['░', '▒', '▓'];
            const maxIntensity = intensity || 2;

            // Apply shading that follows the actual face contour
            for (let y = HAIRLINE + 2; y <= CHIN - 1; y++) {
                const { leftEdge, rightEdge } = this.findFaceBoundary(grid, y, CENTER);

                if (!leftEdge || !rightEdge) continue;

                const faceWidth = rightEdge - leftEdge;
                const midpoint = Math.floor((leftEdge + rightEdge) / 2);

                switch (lightDirection) {
                    case 'topLeft':
                        // Shade from right edge inward
                        for (let i = 1; i <= maxIntensity + 1; i++) {
                            const x = rightEdge - i;
                            if (x > midpoint && grid[y] && grid[y][x] === ' ') {
                                const shadeIdx = Math.min(Math.floor(i / 2), 2);
                                system.setChar(grid, x, y, shadeChars[shadeIdx]);
                            }
                        }
                        // Vertical gradient - more shadow at bottom
                        const bottomFactor = Math.max(0, (y - NOSE_BASE) / (CHIN - NOSE_BASE));
                        if (bottomFactor > 0.3) {
                            for (let x = midpoint + 2; x < rightEdge - 1; x++) {
                                if (grid[y][x] === ' ') {
                                    system.setChar(grid, x, y, '░');
                                }
                            }
                        }
                        break;

                    case 'topRight':
                        // Shade from left edge inward
                        for (let i = 1; i <= maxIntensity + 1; i++) {
                            const x = leftEdge + i;
                            if (x < midpoint && grid[y] && grid[y][x] === ' ') {
                                const shadeIdx = Math.min(Math.floor(i / 2), 2);
                                system.setChar(grid, x, y, shadeChars[shadeIdx]);
                            }
                        }
                        // Vertical gradient
                        const bottomFactorR = Math.max(0, (y - NOSE_BASE) / (CHIN - NOSE_BASE));
                        if (bottomFactorR > 0.3) {
                            for (let x = leftEdge + 1; x < midpoint - 2; x++) {
                                if (grid[y][x] === ' ') {
                                    system.setChar(grid, x, y, '░');
                                }
                            }
                        }
                        break;

                    case 'top':
                        // Shadow under chin area
                        if (y >= NOSE_BASE + 3) {
                            const distFromChin = CHIN - y;
                            if (distFromChin <= 3) {
                                for (let x = leftEdge + 2; x < rightEdge - 2; x++) {
                                    if (grid[y][x] === ' ') {
                                        const shadeIdx = Math.min(3 - distFromChin, 2);
                                        system.setChar(grid, x, y, shadeChars[shadeIdx]);
                                    }
                                }
                            }
                        }
                        // Slight side shadows for roundness
                        if (y >= EYE_LINE && y <= NOSE_BASE + 2) {
                            if (grid[y][leftEdge + 1] === ' ') {
                                system.setChar(grid, leftEdge + 1, y, '░');
                            }
                            if (grid[y][rightEdge - 1] === ' ') {
                                system.setChar(grid, rightEdge - 1, y, '░');
                            }
                        }
                        break;

                    case 'front':
                        // Bilateral subtle shadows on sides
                        const distFromEdge = Math.floor(faceWidth * 0.15);
                        for (let i = 1; i <= distFromEdge; i++) {
                            if (grid[y][leftEdge + i] === ' ') {
                                system.setChar(grid, leftEdge + i, y, '░');
                            }
                            if (grid[y][rightEdge - i] === ' ') {
                                system.setChar(grid, rightEdge - i, y, '░');
                            }
                        }
                        break;
                }
            }
        },

        // Nose shadow - intelligently placed
        noseShadow(grid, system, lightDirection) {
            const { CENTER, NOSE_BASE, EYE_LINE } = system.PROPORTIONS;

            if (lightDirection === 'topLeft') {
                // Shadow on right side of nose bridge
                for (let y = EYE_LINE + 3; y <= NOSE_BASE - 1; y++) {
                    // Only add if space is empty
                    if (grid[y] && grid[y][CENTER + 2] === ' ') {
                        system.setChar(grid, CENTER + 2, y, '░');
                    }
                    if (grid[y] && grid[y][CENTER + 1] === ' ') {
                        system.setChar(grid, CENTER + 1, y, '░');
                    }
                }
                // Shadow cast on upper lip
                if (grid[NOSE_BASE + 1] && grid[NOSE_BASE + 1][CENTER + 1] === ' ') {
                    system.setChar(grid, CENTER + 1, NOSE_BASE + 1, '▒');
                }
            } else if (lightDirection === 'topRight') {
                // Shadow on left side of nose bridge
                for (let y = EYE_LINE + 3; y <= NOSE_BASE - 1; y++) {
                    if (grid[y] && grid[y][CENTER - 2] === ' ') {
                        system.setChar(grid, CENTER - 2, y, '░');
                    }
                    if (grid[y] && grid[y][CENTER - 1] === ' ') {
                        system.setChar(grid, CENTER - 1, y, '░');
                    }
                }
                // Shadow cast on upper lip
                if (grid[NOSE_BASE + 1] && grid[NOSE_BASE + 1][CENTER - 1] === ' ') {
                    system.setChar(grid, CENTER - 1, NOSE_BASE + 1, '▒');
                }
            } else if (lightDirection === 'top') {
                // Shadow directly under nose
                for (let x = CENTER - 1; x <= CENTER + 1; x++) {
                    if (grid[NOSE_BASE + 1] && grid[NOSE_BASE + 1][x] === ' ') {
                        const char = x === CENTER ? '▓' : '▒';
                        system.setChar(grid, x, NOSE_BASE + 1, char);
                    }
                }
                // Subtle shadow under nostrils
                if (grid[NOSE_BASE + 2] && grid[NOSE_BASE + 2][CENTER] === ' ') {
                    system.setChar(grid, CENTER, NOSE_BASE + 2, '░');
                }
            }
        },

        // Cheekbone highlights - dynamically positioned
        cheekboneHighlight(grid, system, lightDirection) {
            const { CENTER, EYE_LINE, NOSE_BASE } = system.PROPORTIONS;
            const cheekY = EYE_LINE + 4;

            // Find cheekbone position based on actual face width at this level
            const { leftEdge, rightEdge } = this.findFaceBoundary(grid, cheekY, CENTER);

            if (!leftEdge || !rightEdge) return;

            const faceWidth = rightEdge - leftEdge;
            const cheekboneOffset = Math.floor(faceWidth * 0.3);

            if (lightDirection === 'topLeft') {
                // Highlight on left cheekbone
                const highlightX = leftEdge + cheekboneOffset;
                for (let i = 0; i < 3; i++) {
                    const y = cheekY + i;
                    const x = highlightX - Math.floor(i / 2);
                    if (grid[y] && grid[y][x] === ' ') {
                        system.setChar(grid, x, y, '·');
                    }
                }
            } else if (lightDirection === 'topRight') {
                // Highlight on right cheekbone
                const highlightX = rightEdge - cheekboneOffset;
                for (let i = 0; i < 3; i++) {
                    const y = cheekY + i;
                    const x = highlightX + Math.floor(i / 2);
                    if (grid[y] && grid[y][x] === ' ') {
                        system.setChar(grid, x, y, '·');
                    }
                }
            } else if (lightDirection === 'top' || lightDirection === 'front') {
                // Bilateral highlights on both cheekbones
                const leftHighlight = leftEdge + cheekboneOffset;
                const rightHighlight = rightEdge - cheekboneOffset;

                for (let i = 0; i < 2; i++) {
                    const y = cheekY + i;
                    if (grid[y] && grid[y][leftHighlight] === ' ') {
                        system.setChar(grid, leftHighlight, y, '·');
                    }
                    if (grid[y] && grid[y][rightHighlight] === ' ') {
                        system.setChar(grid, rightHighlight, y, '·');
                    }
                }
            }
        },

        // Under chin shadow
        chinShadow(grid, system, intensity) {
            const { CENTER, CHIN, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const shadowY = CHIN + 1;
            const shadeChars = ['░', '▒', '▓'];

            for (let x = CENTER - 4; x <= CENTER + 4; x++) {
                if (grid[shadowY] && grid[shadowY][x] === ' ') {
                    system.setChar(grid, x, shadowY, shadeChars[Math.min(intensity - 1, 2)] || '▒');
                }
            }
        },

        // Eye socket depth
        eyeSocketDepth(grid, system) {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;

            // Subtle shading around eyes
            for (let y = EYE_LINE - 1; y <= EYE_LINE + 1; y++) {
                if (grid[y]) {
                    // Left eye socket
                    if (grid[y][EYE_LEFT_CENTER - 3] === ' ') {
                        system.setChar(grid, EYE_LEFT_CENTER - 3, y, '░');
                    }
                    if (grid[y][EYE_LEFT_CENTER + 3] === ' ') {
                        system.setChar(grid, EYE_LEFT_CENTER + 3, y, '░');
                    }
                    // Right eye socket
                    if (grid[y][EYE_RIGHT_CENTER - 3] === ' ') {
                        system.setChar(grid, EYE_RIGHT_CENTER - 3, y, '░');
                    }
                    if (grid[y][EYE_RIGHT_CENTER + 3] === ' ') {
                        system.setChar(grid, EYE_RIGHT_CENTER + 3, y, '░');
                    }
                }
            }
        }
    },

    /**
     * 3/4 VIEW ADJUSTMENTS
     */
    threeFourthView: {
        // Adjust face for left 3/4 view (left side closer to viewer)
        adjustForLeftView(grid, system) {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT, EYE_LINE, NOSE_BASE } = system.PROPORTIONS;

            // Apply graduated shading on far side (right) that follows face contour
            for (let y = HAIRLINE + 2; y <= CHIN - 1; y++) {
                // Find actual face boundary
                let rightEdge = null;
                for (let x = FACE_RIGHT; x >= CENTER; x--) {
                    const char = grid[y] && grid[y][x];
                    if (char && char !== ' ' && char !== '·' && char !== '░' && char !== '▒' && char !== '▓') {
                        rightEdge = x;
                        break;
                    }
                }

                if (!rightEdge) continue;

                // Apply perspective shadow from edge inward
                const shadowDepth = Math.min(6, Math.floor((rightEdge - CENTER) * 0.5));
                for (let i = 1; i <= shadowDepth; i++) {
                    const x = rightEdge - i;
                    if (x > CENTER && grid[y] && grid[y][x] === ' ') {
                        let char = '░';
                        if (i <= 2) char = '░';
                        else if (i <= 4) char = '▒';
                        else char = '▓';
                        system.setChar(grid, x, y, char);
                    }
                }
            }

            // Add edge darkening to create depth
            for (let y = EYE_LINE; y <= NOSE_BASE + 5; y++) {
                if (grid[y] && grid[y][FACE_RIGHT - 1] === ' ') {
                    system.setChar(grid, FACE_RIGHT - 1, y, '▓');
                }
            }
        },

        // Adjust face for right 3/4 view (right side closer to viewer)
        adjustForRightView(grid, system) {
            const { CENTER, HAIRLINE, CHIN, FACE_LEFT, FACE_RIGHT, EYE_LINE, NOSE_BASE } = system.PROPORTIONS;

            // Apply graduated shading on far side (left) that follows face contour
            for (let y = HAIRLINE + 2; y <= CHIN - 1; y++) {
                // Find actual face boundary
                let leftEdge = null;
                for (let x = FACE_LEFT; x <= CENTER; x++) {
                    const char = grid[y] && grid[y][x];
                    if (char && char !== ' ' && char !== '·' && char !== '░' && char !== '▒' && char !== '▓') {
                        leftEdge = x;
                        break;
                    }
                }

                if (!leftEdge) continue;

                // Apply perspective shadow from edge inward
                const shadowDepth = Math.min(6, Math.floor((CENTER - leftEdge) * 0.5));
                for (let i = 1; i <= shadowDepth; i++) {
                    const x = leftEdge + i;
                    if (x < CENTER && grid[y] && grid[y][x] === ' ') {
                        let char = '░';
                        if (i <= 2) char = '░';
                        else if (i <= 4) char = '▒';
                        else char = '▓';
                        system.setChar(grid, x, y, char);
                    }
                }
            }

            // Add edge darkening to create depth
            for (let y = EYE_LINE; y <= NOSE_BASE + 5; y++) {
                if (grid[y] && grid[y][FACE_LEFT + 1] === ' ') {
                    system.setChar(grid, FACE_LEFT + 1, y, '▓');
                }
            }
        }
    },

    /**
     * AGING FEATURES
     */
    agingFeatures: {
        foreheadLines: (grid, system, intensity) => {
            const { CENTER, HAIRLINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const lines = intensity || 2;
            for (let i = 0; i < lines; i++) {
                const y = HAIRLINE + 2 + (i * 2);
                for (let x = FACE_LEFT + 4; x <= FACE_RIGHT - 4; x += 2) {
                    system.setChar(grid, x, y, '‾');
                }
            }
        },

        crowsFeet: (grid, system) => {
            const { EYE_LINE, EYE_LEFT_CENTER, EYE_RIGHT_CENTER } = system.PROPORTIONS;
            // Left eye
            system.setChar(grid, EYE_LEFT_CENTER - 3, EYE_LINE - 1, '\\');
            system.setChar(grid, EYE_LEFT_CENTER - 3, EYE_LINE, '─');
            system.setChar(grid, EYE_LEFT_CENTER - 3, EYE_LINE + 1, '/');

            // Right eye
            system.setChar(grid, EYE_RIGHT_CENTER + 3, EYE_LINE - 1, '/');
            system.setChar(grid, EYE_RIGHT_CENTER + 3, EYE_LINE, '─');
            system.setChar(grid, EYE_RIGHT_CENTER + 3, EYE_LINE + 1, '\\');
        },

        laughLines: (grid, system) => {
            const { CENTER, NOSE_BASE, MOUTH_CENTER, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            // Nasolabial folds
            system.setChar(grid, CENTER - 3, NOSE_BASE + 1, '(');
            system.setChar(grid, CENTER - 4, NOSE_BASE + 2, '(');
            system.setChar(grid, CENTER - 4, MOUTH_CENTER, '(');

            system.setChar(grid, CENTER + 3, NOSE_BASE + 1, ')');
            system.setChar(grid, CENTER + 4, NOSE_BASE + 2, ')');
            system.setChar(grid, CENTER + 4, MOUTH_CENTER, ')');
        },

        ageSpots: (grid, system, count) => {
            const { FACE_LEFT, FACE_RIGHT, HAIRLINE, CHIN } = system.PROPORTIONS;
            const spots = count || 3;
            const positions = [
                [FACE_LEFT + 5, HAIRLINE + 5],
                [FACE_RIGHT - 5, HAIRLINE + 7],
                [FACE_LEFT + 7, HAIRLINE + 10],
                [FACE_RIGHT - 6, HAIRLINE + 8],
                [FACE_LEFT + 8, HAIRLINE + 12]
            ];

            for (let i = 0; i < Math.min(spots, positions.length); i++) {
                const [x, y] = positions[i];
                system.setChar(grid, x, y, '·');
            }
        },

        freckles(grid, system, density) {
            const { CENTER, NOSE_BASE, EYE_LINE, FACE_LEFT, FACE_RIGHT } = system.PROPORTIONS;
            const count = density === 'heavy' ? 15 : density === 'medium' ? 8 : 5;

            // Use fixed seeded positions so freckles don't move during animation
            const faceWidth = FACE_RIGHT - FACE_LEFT - 8;
            const faceHeight = NOSE_BASE - EYE_LINE + 3;

            // Pre-generated static positions using a simple pattern
            const staticPositions = [];
            for (let i = 0; i < 15; i++) {
                // Use a deterministic pattern based on index
                const seed = i * 37; // Prime number for better distribution
                const xOffset = (seed * 17) % faceWidth;
                const yOffset = (seed * 13) % faceHeight;
                staticPositions.push({
                    x: FACE_LEFT + 4 + xOffset,
                    y: EYE_LINE + yOffset
                });
            }

            for (let i = 0; i < count; i++) {
                const pos = staticPositions[i];
                if (grid[pos.y] && grid[pos.y][pos.x] === ' ') {
                    system.setChar(grid, pos.x, pos.y, '·');
                }
            }
        },

        saggingFeatures: (grid, system) => {
            const { CENTER, CHIN, MOUTH_CENTER } = system.PROPORTIONS;
            // Jowls
            system.setChar(grid, CENTER - 5, MOUTH_CENTER + 3, '(');
            system.setChar(grid, CENTER - 4, MOUTH_CENTER + 4, '(');
            system.setChar(grid, CENTER + 5, MOUTH_CENTER + 3, ')');
            system.setChar(grid, CENTER + 4, MOUTH_CENTER + 4, ')');
        }
    }

    // More features can be added: hair, facial hair, etc.
};
