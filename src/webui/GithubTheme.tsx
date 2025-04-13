
// dark colors from the vim theme
// and i picked the light colors
// roughly based off the classic theme from https://github.com/primer/github-vscode-theme

import { tags as t } from "@lezer/highlight"
import { EditorView } from "codemirror";
import { HighlightStyle, syntaxHighlighting } from "@codemirror/language"
import { Extension } from "@codemirror/state";

// TODO: decouple this to only have the colors used in the CSS
export interface Colors {
    base0: string;
    base1: string;
    base2: string;
    base3: string;
    base4: string;
    base5: string;
    red: string;
    orange: string;
    bgorange: string;
    green: string;
    bggreen: string;
    lightblue: string;
    blue: string;
    purp: string;
};

export interface Theme {
    colors: Colors,
    cmTheme: Extension,
};

const darkColors: Colors = {
    base0: "#0d1117",
    base1: "#161b22",
    base2: "#21262d",
    base3: "#89929b",
    base4: "#c6cdd5",
    base5: "#ecf2f8",
    red: "#fa7970",
    orange: "#faa356",
    bgorange: "#802000",
    green: "#7ce38b",
    lightblue: "#a2d2fb",
    blue: "#77bdfb",
    purp: "#cea5fb",
    bggreen: "#116329"
};

const lightColors = {
    base0: "#fefefe",
    base1: "#f6f8fa",
    base2: "#d1d5da",
    base3: "#959da5",
    base4: "#586069",
    base5: "#24292e",
    purp: "#8250df",
    red: "#cf222e",
    orange: "#953800",
    blue: "#0a3069",
    lightblue: "#0550ae",
    comment: "#59636e",
    green: "#116329",
    bgorange: "#fac080",
    bggreen: "#7ce38b"
};

const cssTheme = (dark: boolean) => {
    const colors = dark ? darkColors : lightColors;
    return EditorView.theme({
        "&": {
            color: colors.base5,
            backgroundColor: colors.base0
        },

        ".cm-content": {
            caretColor: colors.blue
        },

        ".cm-cursor, .cm-dropCursor": { borderLeftColor: colors.blue },
        "&.cm-focused > .cm-scroller > .cm-selectionLayer .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection": { backgroundColor: colors.base2 },
        ".cm-activeLine": {  "background-color": colors.base2+"28" },
        ".cm-content ::selection .cm-activeLine": { backgroundColor: colors.base3 },

        ".cm-panels": { backgroundColor: colors.base0, color: colors.base5 },
        ".cm-panels.cm-panels-top": { borderBottom: "2px solid black" },
        ".cm-panels.cm-panels-bottom": { borderTop: "2px solid black" },

        ".cm-searchMatch": {
            backgroundColor: "#72a1ff59",
            outline: "1px solid #457dff"
        },
        ".cm-searchMatch.cm-searchMatch-selected": {
            backgroundColor: "#6199ff2f"
        },

        ".cm-selectionMatch": { backgroundColor: "#aafe661a" },

        "&.cm-focused .cm-matchingBracket, &.cm-focused .cm-nonmatchingBracket": {
            backgroundColor: "#bad0f847"
        },

        ".cm-gutters": {
            backgroundColor: colors.base0,
            color: colors.base4,
            border: "none"
        },

        ".cm-activeLineGutter": {
            backgroundColor: colors.base1
        },

        ".cm-foldPlaceholder": {
            backgroundColor: "transparent",
            border: "none",
            color: "#ddd"
        },
        ".cm-textfield": {
            backgroundColor: colors.base2,
            backgroundImage: "none",
            border: "none",
        },
        ".cm-button": {
            backgroundColor: colors.base2,
            backgroundImage: "none",
            border: "none",
        },

        ".cm-search > label": {
            "display": "flex",
            "align-items": "center"
        },
        ".cm-search > br": {
            "display": "none",
        },
        ".cm-panel.cm-search input[type=checkbox]": {
            "-webkit-appearance": "none",
            "-moz-appearance": "none",
            "appearance": "none",
            "width": "20px",
            "margin": "5px",
            "height": "20px",
            "border": "none",
            "background-color": colors.base2,
            "cursor": "pointer",
        },

        ".cm-panel.cm-search input[type=checkbox]:hover": {
            "background-color": colors.base3,
        },

        ".cm-panel.cm-search input[type=checkbox]:checked": {
            "background-color": colors.base5,
        },

        ".cm-panel.cm-search input[type=checkbox]:checked:hover": {
            "background-color": colors.base4,
        },

        ".cm-search > button:hover": {
            "background-color": colors.base3,
            "background-image": "none",
        },

        ".cm-search > button:active": {
            "background-color": colors.base5,
            "color": colors.base0,
            "background-image": "none",
        },

        ".cm-search > button:active:hover": {
            "background-color": colors.base4,
            "color": colors.base0,
            "background-image": "none",
        },

        ".cm-tooltip": {
            border: "none",
            backgroundColor: colors.base3
        },
        ".cm-tooltip .cm-tooltip-arrow:before": {
            borderTopColor: "transparent",
            borderBottomColor: "transparent"
        },
        ".cm-tooltip .cm-tooltip-arrow:after": {
            borderTopColor: colors.base3,
            borderBottomColor: colors.base3
        },

    }, { dark: dark });
}

const highlightStyle = colors => HighlightStyle.define([
    {
        tag: t.keyword,
        color: colors.purp
    },
    {
        tag: [t.name, t.deleted, t.character, t.propertyName, t.macroName],
        color: colors.red
    },
    {
        tag: [t.function(t.variableName), t.labelName],
        color: colors.blue
    },
    {
        tag: [t.color, t.constant(t.name), t.standard(t.name)],
        color: colors.orange
    },
    {
        tag: [t.definition(t.name), t.separator],
        color: colors.base4
    },
    {
        tag: [t.typeName, t.className, t.number, t.changed, t.annotation, t.modifier, t.self, t.namespace],
        color: colors.orange
    },
    {
        tag: [t.operator, t.operatorKeyword, t.url, t.escape, t.regexp, t.link, t.special(t.string)],
        color: colors.lightblue
    },
    {
        tag: [t.meta, t.comment],
        color: colors.base3
    },
    {
        tag: t.strong,
        fontWeight: "bold"
    },
    {
        tag: t.emphasis,
        fontStyle: "italic"
    },
    {
        tag: t.strikethrough,
        textDecoration: "line-through"
    },
    {
        tag: t.link,
        color: colors.base3,
        textDecoration: "underline"
    },
    {
        tag: t.heading,
        fontWeight: "bold",
        color: colors.red
    },
    {
        tag: [t.atom, t.bool, t.special(t.variableName)],
        color: colors.orange
    },
    {
        tag: [t.processingInstruction, t.string, t.inserted],
        color: colors.green
    },
    {
        tag: t.invalid,
        color: colors.base5
    },
])


export const githubLight: Theme = { colors: lightColors, cmTheme: [cssTheme(false), syntaxHighlighting(highlightStyle(lightColors))] };
export const githubDark: Theme = { colors: darkColors, cmTheme: [cssTheme(true), syntaxHighlighting(highlightStyle(darkColors))] };
