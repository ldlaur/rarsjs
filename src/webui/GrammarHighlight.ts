import {styleTags, tags as t} from "@lezer/highlight"

export const highlighting = styleTags({
    Instruction: t.operatorKeyword,
    Number: t.number,
    Register: t.variableName,
    Directive: t.tagName,
    String: t.string,
    LineComment: t.comment,
    BlockComment: t.comment,
});