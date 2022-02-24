import { StyleSheet } from "react-native";
import colors from "./colors";

export const buttonStyles: StyleSheet.NamedStyles<any> = {
  button: {
    paddingVertical: 10,
    paddingHorizontal: 20,
    justifyContent: "center",
    alignItems: "center",
    marginLeft: 20,
    borderRadius: 5,
    backgroundColor: colors.purple,
  },
  text: {
    color: colors.white,
    textAlign: "center",
    fontSize: 17,
    fontWeight: "bold",
  },
}