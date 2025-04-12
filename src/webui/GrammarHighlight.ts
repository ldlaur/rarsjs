import { styleTags, tags as t } from "@lezer/highlight";

export const highlighting = styleTags({
  Instruction: t.controlKeyword,
  Number: t.number,
  Register: t.variableName,
  Directive: t.operator,
  String: t.string,
  LineComment: t.comment,
  BlockComment: t.comment,
});
